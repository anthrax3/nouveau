/*
 * Copyright 2012 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Ben Skeggs
 */
#include "nv50.h"
#include "head.h"
#include "ior.h"
#include "rootnv50.h"

#include <core/client.h>
#include <core/enum.h>
#include <core/gpuobj.h>
#include <subdev/bios.h>
#include <subdev/bios/disp.h>
#include <subdev/bios/init.h>
#include <subdev/bios/pll.h>
#include <subdev/devinit.h>
#include <subdev/timer.h>

static const struct nvkm_disp_oclass *
nv50_disp_root_(struct nvkm_disp *base)
{
	return nv50_disp(base)->func->root;
}

static void
nv50_disp_intr_(struct nvkm_disp *base)
{
	struct nv50_disp *disp = nv50_disp(base);
	disp->func->intr(disp);
}

static void *
nv50_disp_dtor_(struct nvkm_disp *base)
{
	struct nv50_disp *disp = nv50_disp(base);
	nvkm_event_fini(&disp->uevent);
	if (disp->wq)
		destroy_workqueue(disp->wq);
	return disp;
}

static const struct nvkm_disp_func
nv50_disp_ = {
	.dtor = nv50_disp_dtor_,
	.intr = nv50_disp_intr_,
	.root = nv50_disp_root_,
};

int
nv50_disp_new_(const struct nv50_disp_func *func, struct nvkm_device *device,
	       int index, int heads, struct nvkm_disp **pdisp)
{
	struct nv50_disp *disp;
	int ret, i;

	if (!(disp = kzalloc(sizeof(*disp), GFP_KERNEL)))
		return -ENOMEM;
	disp->func = func;
	*pdisp = &disp->base;

	ret = nvkm_disp_ctor(&nv50_disp_, device, index, &disp->base);
	if (ret)
		return ret;

	disp->wq = create_singlethread_workqueue("nvkm-disp");
	if (!disp->wq)
		return -ENOMEM;
	INIT_WORK(&disp->supervisor, func->super);

	for (i = 0; func->head.new && i < heads; i++) {
		ret = func->head.new(&disp->base, i);
		if (ret)
			return ret;
	}

	for (i = 0; func->dac.new && i < func->dac.nr; i++) {
		ret = func->dac.new(&disp->base, i);
		if (ret)
			return ret;
	}

	for (i = 0; func->pior.new && i < func->pior.nr; i++) {
		ret = func->pior.new(&disp->base, i);
		if (ret)
			return ret;
	}

	for (i = 0; func->sor.new && i < func->sor.nr; i++) {
		ret = func->sor.new(&disp->base, i);
		if (ret)
			return ret;
	}

	return nvkm_event_init(func->uevent, 1, 1 + (heads * 4), &disp->uevent);
}

static u32
nv50_disp_super_iedt(struct nvkm_head *head, struct nvkm_outp *outp,
		     u8 *ver, u8 *hdr, u8 *cnt, u8 *len,
		     struct nvbios_outp *iedt)
{
	struct nvkm_bios *bios = head->disp->engine.subdev.device->bios;
	const u8  l = ffs(outp->info.link);
	const u16 t = outp->info.hasht;
	const u16 m = (0x0100 << head->id) | (l << 6) | outp->info.or;
	u32 data = nvbios_outp_match(bios, t, m, ver, hdr, cnt, len, iedt);
	if (!data)
		OUTP_DBG(outp, "missing IEDT for %04x:%04x", t, m);
	return data;
}

static void
nv50_disp_super_ied_off(struct nvkm_head *head, struct nvkm_ior *ior, int id)
{
	struct nvkm_outp *outp = ior->arm.outp;
	struct nvbios_outp iedt;
	u8  ver, hdr, cnt, len;
	u32 data;

	if (!outp) {
		IOR_DBG(ior, "nothing attached");
		return;
	}

	data = nv50_disp_super_iedt(head, outp, &ver, &hdr, &cnt, &len, &iedt);
	if (!data)
		return;

	nvbios_init(&head->disp->engine.subdev, iedt.script[id],
		init.outp = &outp->info;
		init.or   = ior->id;
		init.link = ior->arm.link;
		init.head = head->id;
	);
}

static struct nvkm_ior *
nv50_disp_super_ior_arm(struct nvkm_head *head)
{
	struct nvkm_ior *ior;
	list_for_each_entry(ior, &head->disp->ior, head) {
		if (ior->arm.head & (1 << head->id)) {
			HEAD_DBG(head, "on %s", ior->name);
			return ior;
		}
	}
	HEAD_DBG(head, "nothing attached");
	return NULL;
}

static struct nvkm_output *
exec_lookup(struct nv50_disp *disp, int head, int or, u32 ctrl,
	    u32 *data, u8 *ver, u8 *hdr, u8 *cnt, u8 *len,
	    struct nvbios_outp *info)
{
	struct nvkm_subdev *subdev = &disp->base.engine.subdev;
	struct nvkm_bios *bios = subdev->device->bios;
	struct nvkm_output *outp;
	u16 mask, type;

	if (or < 4) {
		type = DCB_OUTPUT_ANALOG;
		mask = 0;
	} else
	if (or < 8) {
		switch (ctrl & 0x00000f00) {
		case 0x00000000: type = DCB_OUTPUT_LVDS; mask = 1; break;
		case 0x00000100: type = DCB_OUTPUT_TMDS; mask = 1; break;
		case 0x00000200: type = DCB_OUTPUT_TMDS; mask = 2; break;
		case 0x00000500: type = DCB_OUTPUT_TMDS; mask = 3; break;
		case 0x00000800: type = DCB_OUTPUT_DP; mask = 1; break;
		case 0x00000900: type = DCB_OUTPUT_DP; mask = 2; break;
		default:
			nvkm_error(subdev, "unknown SOR mc %08x\n", ctrl);
			return NULL;
		}
		or  -= 4;
	} else {
		or   = or - 8;
		type = 0x0010;
		mask = 0;
		switch (ctrl & 0x00000f00) {
		case 0x00000000: type |= disp->pior.type[or]; break;
		default:
			nvkm_error(subdev, "unknown PIOR mc %08x\n", ctrl);
			return NULL;
		}
	}

	mask  = 0x00c0 & (mask << 6);
	mask |= 0x0001 << or;
	mask |= 0x0100 << head;

	list_for_each_entry(outp, &disp->base.outp, head) {
		if ((outp->info.hasht & 0xff) == type &&
		    (outp->info.hashm & mask) == mask) {
			*data = nvbios_outp_match(bios, outp->info.hasht, mask,
						  ver, hdr, cnt, len, info);
			if (!*data)
				return NULL;
			return outp;
		}
	}

	return NULL;
}

static struct nvkm_output *
exec_clkcmp(struct nv50_disp *disp, int head, int id, u32 pclk, u32 *conf)
{
	struct nvkm_subdev *subdev = &disp->base.engine.subdev;
	struct nvkm_device *device = subdev->device;
	struct nvkm_bios *bios = device->bios;
	struct nvkm_output *outp;
	struct nvbios_outp info1;
	struct nvbios_ocfg info2;
	u8  ver, hdr, cnt, len;
	u32 data, ctrl = 0;
	u32 reg;
	int i;

	/* DAC */
	for (i = 0; !(ctrl & (1 << head)) && i < disp->func->dac.nr; i++)
		ctrl = nvkm_rd32(device, 0x610b58 + (i * 8));

	/* SOR */
	if (!(ctrl & (1 << head))) {
		if (device->chipset  < 0x90 ||
		    device->chipset == 0x92 ||
		    device->chipset == 0xa0) {
			reg = 0x610b70;
		} else {
			reg = 0x610794;
		}
		for (i = 0; !(ctrl & (1 << head)) && i < disp->func->sor.nr; i++)
			ctrl = nvkm_rd32(device, reg + (i * 8));
		i += 4;
	}

	/* PIOR */
	if (!(ctrl & (1 << head))) {
		for (i = 0; !(ctrl & (1 << head)) && i < disp->func->pior.nr; i++)
			ctrl = nvkm_rd32(device, 0x610b80 + (i * 8));
		i += 8;
	}

	if (!(ctrl & (1 << head)))
		return NULL;
	i--;

	outp = exec_lookup(disp, head, i, ctrl, &data, &ver, &hdr, &cnt, &len, &info1);
	if (!outp)
		return NULL;

	*conf = (ctrl & 0x00000f00) >> 8;
	if (outp->info.location == 0) {
		switch (outp->info.type) {
		case DCB_OUTPUT_TMDS:
			if (*conf == 5)
				*conf |= 0x0100;
			break;
		case DCB_OUTPUT_LVDS:
			*conf |= disp->sor.lvdsconf;
			break;
		default:
			break;
		}
	} else {
		*conf = (ctrl & 0x00000f00) >> 8;
		pclk = pclk / 2;
	}

	data = nvbios_ocfg_match(bios, data, *conf & 0xff, *conf >> 8,
				 &ver, &hdr, &cnt, &len, &info2);
	if (data && id < 0xff) {
		data = nvbios_oclk_match(bios, info2.clkcmp[id], pclk);
		if (data) {
			struct nvbios_init init = {
				.subdev = subdev,
				.bios = bios,
				.offset = data,
				.outp = &outp->info,
				.crtc = head,
				.execute = 1,
			};

			nvbios_exec(&init);
		}
	}

	return outp;
}

static void
nv50_disp_intr_unk40_0(struct nv50_disp *disp, int head)
{
	struct nvkm_device *device = disp->base.engine.subdev.device;
	struct nvkm_output *outp;
	u32 pclk = nvkm_rd32(device, 0x610ad0 + (head * 0x540)) & 0x3fffff;
	u32 conf;

	outp = exec_clkcmp(disp, head, 1, pclk, &conf);
	if (!outp)
		return;

	nv50_disp_dptmds_war_3(disp, &outp->info);
}

static void
nv50_disp_intr_unk20_2_dp(struct nv50_disp *disp, int head,
			  struct dcb_output *outp, u32 pclk)
{
	struct nvkm_subdev *subdev = &disp->base.engine.subdev;
	struct nvkm_device *device = subdev->device;
	const int link = !(outp->sorconf.link & 1);
	const int   or = ffs(outp->or) - 1;
	const u32 soff = (  or * 0x800);
	const u32 loff = (link * 0x080) + soff;
	const u32 ctrl = nvkm_rd32(device, 0x610794 + (or * 8));
	const u32 symbol = 100000;
	const s32 vactive = nvkm_rd32(device, 0x610af8 + (head * 0x540)) & 0xffff;
	const s32 vblanke = nvkm_rd32(device, 0x610ae8 + (head * 0x540)) & 0xffff;
	const s32 vblanks = nvkm_rd32(device, 0x610af0 + (head * 0x540)) & 0xffff;
	u32 dpctrl = nvkm_rd32(device, 0x61c10c + loff);
	u32 clksor = nvkm_rd32(device, 0x614300 + soff);
	int bestTU = 0, bestVTUi = 0, bestVTUf = 0, bestVTUa = 0;
	int TU, VTUi, VTUf, VTUa;
	u64 link_data_rate, link_ratio, unk;
	u32 best_diff = 64 * symbol;
	u32 link_nr, link_bw, bits;
	u64 value;

	link_bw = (clksor & 0x000c0000) ? 270000 : 162000;
	link_nr = hweight32(dpctrl & 0x000f0000);

	/* symbols/hblank - algorithm taken from comments in tegra driver */
	value = vblanke + vactive - vblanks - 7;
	value = value * link_bw;
	do_div(value, pclk);
	value = value - (3 * !!(dpctrl & 0x00004000)) - (12 / link_nr);
	nvkm_mask(device, 0x61c1e8 + soff, 0x0000ffff, value);

	/* symbols/vblank - algorithm taken from comments in tegra driver */
	value = vblanks - vblanke - 25;
	value = value * link_bw;
	do_div(value, pclk);
	value = value - ((36 / link_nr) + 3) - 1;
	nvkm_mask(device, 0x61c1ec + soff, 0x00ffffff, value);

	/* watermark / activesym */
	if      ((ctrl & 0xf0000) == 0x60000) bits = 30;
	else if ((ctrl & 0xf0000) == 0x50000) bits = 24;
	else                                  bits = 18;

	link_data_rate = (pclk * bits / 8) / link_nr;

	/* calculate ratio of packed data rate to link symbol rate */
	link_ratio = link_data_rate * symbol;
	do_div(link_ratio, link_bw);

	for (TU = 64; TU >= 32; TU--) {
		/* calculate average number of valid symbols in each TU */
		u32 tu_valid = link_ratio * TU;
		u32 calc, diff;

		/* find a hw representation for the fraction.. */
		VTUi = tu_valid / symbol;
		calc = VTUi * symbol;
		diff = tu_valid - calc;
		if (diff) {
			if (diff >= (symbol / 2)) {
				VTUf = symbol / (symbol - diff);
				if (symbol - (VTUf * diff))
					VTUf++;

				if (VTUf <= 15) {
					VTUa  = 1;
					calc += symbol - (symbol / VTUf);
				} else {
					VTUa  = 0;
					VTUf  = 1;
					calc += symbol;
				}
			} else {
				VTUa  = 0;
				VTUf  = min((int)(symbol / diff), 15);
				calc += symbol / VTUf;
			}

			diff = calc - tu_valid;
		} else {
			/* no remainder, but the hw doesn't like the fractional
			 * part to be zero.  decrement the integer part and
			 * have the fraction add a whole symbol back
			 */
			VTUa = 0;
			VTUf = 1;
			VTUi--;
		}

		if (diff < best_diff) {
			best_diff = diff;
			bestTU = TU;
			bestVTUa = VTUa;
			bestVTUf = VTUf;
			bestVTUi = VTUi;
			if (diff == 0)
				break;
		}
	}

	if (!bestTU) {
		nvkm_error(subdev, "unable to find suitable dp config\n");
		return;
	}

	/* XXX close to vbios numbers, but not right */
	unk  = (symbol - link_ratio) * bestTU;
	unk *= link_ratio;
	do_div(unk, symbol);
	do_div(unk, symbol);
	unk += 6;

	nvkm_mask(device, 0x61c10c + loff, 0x000001fc, bestTU << 2);
	nvkm_mask(device, 0x61c128 + loff, 0x010f7f3f, bestVTUa << 24 |
						   bestVTUf << 16 |
						   bestVTUi << 8 | unk);
}

static void
nv50_disp_intr_unk20_2(struct nv50_disp *disp, int head)
{
	struct nvkm_device *device = disp->base.engine.subdev.device;
	struct nvkm_output *outp;
	u32 pclk = nvkm_rd32(device, 0x610ad0 + (head * 0x540)) & 0x3fffff;
	u32 hval, hreg = 0x614200 + (head * 0x800);
	u32 oval, oreg;
	u32 mask, conf;

	outp = exec_clkcmp(disp, head, 0xff, pclk, &conf);
	if (!outp)
		return;

	/* we allow both encoder attach and detach operations to occur
	 * within a single supervisor (ie. modeset) sequence.  the
	 * encoder detach scripts quite often switch off power to the
	 * lanes, which requires the link to be re-trained.
	 *
	 * this is not generally an issue as the sink "must" (heh)
	 * signal an irq when it's lost sync so the driver can
	 * re-train.
	 *
	 * however, on some boards, if one does not configure at least
	 * the gpu side of the link *before* attaching, then various
	 * things can go horribly wrong (PDISP disappearing from mmio,
	 * third supervisor never happens, etc).
	 *
	 * the solution is simply to retrain here, if necessary.  last
	 * i checked, the binary driver userspace does not appear to
	 * trigger this situation (it forces an UPDATE between steps).
	 */
	if (outp->info.type == DCB_OUTPUT_DP) {
		u32 soff = (ffs(outp->info.or) - 1) * 0x08;
		u32 ctrl, datarate;

		if (outp->info.location == 0) {
			ctrl = nvkm_rd32(device, 0x610794 + soff);
			soff = 1;
		} else {
			ctrl = nvkm_rd32(device, 0x610b80 + soff);
			soff = 2;
		}

		switch ((ctrl & 0x000f0000) >> 16) {
		case 6: datarate = pclk * 30; break;
		case 5: datarate = pclk * 24; break;
		case 2:
		default:
			datarate = pclk * 18;
			break;
		}

		if (nvkm_output_dp_train(outp, datarate / soff))
			OUTP_ERR(outp, "link not trained before attach");
	}

	exec_clkcmp(disp, head, 0, pclk, &conf);

	if (!outp->info.location && outp->info.type == DCB_OUTPUT_ANALOG) {
		oreg = 0x614280 + (ffs(outp->info.or) - 1) * 0x800;
		oval = 0x00000000;
		hval = 0x00000000;
		mask = 0xffffffff;
	} else
	if (!outp->info.location) {
		if (outp->info.type == DCB_OUTPUT_DP)
			nv50_disp_intr_unk20_2_dp(disp, head, &outp->info, pclk);
		oreg = 0x614300 + (ffs(outp->info.or) - 1) * 0x800;
		oval = (conf & 0x0100) ? 0x00000101 : 0x00000000;
		hval = 0x00000000;
		mask = 0x00000707;
	} else {
		oreg = 0x614380 + (ffs(outp->info.or) - 1) * 0x800;
		oval = 0x00000001;
		hval = 0x00000001;
		mask = 0x00000707;
	}

	nvkm_mask(device, hreg, 0x0000000f, hval);
	nvkm_mask(device, oreg, mask, oval);

	nv50_disp_dptmds_war_2(disp, &outp->info);
}

void
nv50_disp_super_2_1(struct nv50_disp *disp, struct nvkm_head *head)
{
	struct nvkm_devinit *devinit = disp->base.engine.subdev.device->devinit;
	u32 khz = head->asy.hz / 1000;
	HEAD_DBG(head, "supervisor 2.1 - %d khz", khz);
	if (khz)
		nvkm_devinit_pll_set(devinit, PLL_VPLL0 + head->id, khz);
}

void
nv50_disp_super_2_0(struct nv50_disp *disp, struct nvkm_head *head)
{
	struct nvkm_outp *outp;
	struct nvkm_ior *ior;

	/* Determine which OR, if any, we're detaching from the head. */
	HEAD_DBG(head, "supervisor 2.0");
	ior = nv50_disp_super_ior_arm(head);
	if (!ior)
		return;

	/* Execute OffInt2 IED script. */
	nv50_disp_super_ied_off(head, ior, 2);

	/* If we're shutting down the OR's only active head, execute
	 * the output path's release function.
	 */
	if (ior->arm.head == (1 << head->id)) {
		if ((outp = ior->arm.outp) && outp->func->release)
			outp->func->release(outp, ior);
	}
}

void
nv50_disp_super_1_0(struct nv50_disp *disp, struct nvkm_head *head)
{
	struct nvkm_ior *ior;

	/* Determine which OR, if any, we're detaching from the head. */
	HEAD_DBG(head, "supervisor 1.0");
	ior = nv50_disp_super_ior_arm(head);
	if (!ior)
		return;

	/* Execute OffInt1 IED script. */
	nv50_disp_super_ied_off(head, ior, 1);
}

void
nv50_disp_super_1(struct nv50_disp *disp)
{
	struct nvkm_head *head;
	struct nvkm_ior *ior;

	list_for_each_entry(head, &disp->base.head, head) {
		head->func->state(head, &head->arm);
		head->func->state(head, &head->asy);
	}

	list_for_each_entry(ior, &disp->base.ior, head) {
		ior->func->state(ior, &ior->arm);
		ior->func->state(ior, &ior->asy);
	}
}

void
nv50_disp_super(struct work_struct *work)
{
	struct nv50_disp *disp =
		container_of(work, struct nv50_disp, supervisor);
	struct nvkm_subdev *subdev = &disp->base.engine.subdev;
	struct nvkm_device *device = subdev->device;
	struct nvkm_head *head;
	u32 super = nvkm_rd32(device, 0x610030);

	nvkm_debug(subdev, "supervisor %08x %08x\n", disp->super, super);

	if (disp->super & 0x00000010) {
		nv50_disp_chan_mthd(disp->chan[0], NV_DBG_DEBUG);
		nv50_disp_super_1(disp);
		list_for_each_entry(head, &disp->base.head, head) {
			if (!(super & (0x00000020 << head->id)))
				continue;
			if (!(super & (0x00000080 << head->id)))
				continue;
			nv50_disp_super_1_0(disp, head);
		}
	} else
	if (disp->super & 0x00000020) {
		list_for_each_entry(head, &disp->base.head, head) {
			if (!(super & (0x00000080 << head->id)))
				continue;
			nv50_disp_super_2_0(disp, head);
		}
		nvkm_outp_route(&disp->base);
		list_for_each_entry(head, &disp->base.head, head) {
			if (!(super & (0x00000200 << head->id)))
				continue;
			nv50_disp_super_2_1(disp, head);
		}
		list_for_each_entry(head, &disp->base.head, head) {
			if (!(super & (0x00000080 << head->id)))
				continue;
			nv50_disp_intr_unk20_2(disp, head->id);
		}
	} else
	if (disp->super & 0x00000040) {
		list_for_each_entry(head, &disp->base.head, head) {
			if (!(super & (0x00000080 << head->id)))
				continue;
			nv50_disp_intr_unk40_0(disp, head->id);
		}
		nv50_disp_update_sppll1(disp);
	}

	nvkm_wr32(device, 0x610030, 0x80000000);
}

static const struct nvkm_enum
nv50_disp_intr_error_type[] = {
	{ 3, "ILLEGAL_MTHD" },
	{ 4, "INVALID_VALUE" },
	{ 5, "INVALID_STATE" },
	{ 7, "INVALID_HANDLE" },
	{}
};

static const struct nvkm_enum
nv50_disp_intr_error_code[] = {
	{ 0x00, "" },
	{}
};

static void
nv50_disp_intr_error(struct nv50_disp *disp, int chid)
{
	struct nvkm_subdev *subdev = &disp->base.engine.subdev;
	struct nvkm_device *device = subdev->device;
	u32 data = nvkm_rd32(device, 0x610084 + (chid * 0x08));
	u32 addr = nvkm_rd32(device, 0x610080 + (chid * 0x08));
	u32 code = (addr & 0x00ff0000) >> 16;
	u32 type = (addr & 0x00007000) >> 12;
	u32 mthd = (addr & 0x00000ffc);
	const struct nvkm_enum *ec, *et;

	et = nvkm_enum_find(nv50_disp_intr_error_type, type);
	ec = nvkm_enum_find(nv50_disp_intr_error_code, code);

	nvkm_error(subdev,
		   "ERROR %d [%s] %02x [%s] chid %d mthd %04x data %08x\n",
		   type, et ? et->name : "", code, ec ? ec->name : "",
		   chid, mthd, data);

	if (chid < ARRAY_SIZE(disp->chan)) {
		switch (mthd) {
		case 0x0080:
			nv50_disp_chan_mthd(disp->chan[chid], NV_DBG_ERROR);
			break;
		default:
			break;
		}
	}

	nvkm_wr32(device, 0x610020, 0x00010000 << chid);
	nvkm_wr32(device, 0x610080 + (chid * 0x08), 0x90000000);
}

void
nv50_disp_intr(struct nv50_disp *disp)
{
	struct nvkm_device *device = disp->base.engine.subdev.device;
	u32 intr0 = nvkm_rd32(device, 0x610020);
	u32 intr1 = nvkm_rd32(device, 0x610024);

	while (intr0 & 0x001f0000) {
		u32 chid = __ffs(intr0 & 0x001f0000) - 16;
		nv50_disp_intr_error(disp, chid);
		intr0 &= ~(0x00010000 << chid);
	}

	while (intr0 & 0x0000001f) {
		u32 chid = __ffs(intr0 & 0x0000001f);
		nv50_disp_chan_uevent_send(disp, chid);
		intr0 &= ~(0x00000001 << chid);
	}

	if (intr1 & 0x00000004) {
		nvkm_disp_vblank(&disp->base, 0);
		nvkm_wr32(device, 0x610024, 0x00000004);
	}

	if (intr1 & 0x00000008) {
		nvkm_disp_vblank(&disp->base, 1);
		nvkm_wr32(device, 0x610024, 0x00000008);
	}

	if (intr1 & 0x00000070) {
		disp->super = (intr1 & 0x00000070);
		queue_work(disp->wq, &disp->supervisor);
		nvkm_wr32(device, 0x610024, disp->super);
	}
}

static const struct nv50_disp_func
nv50_disp = {
	.intr = nv50_disp_intr,
	.uevent = &nv50_disp_chan_uevent,
	.super = nv50_disp_super,
	.root = &nv50_disp_root_oclass,
	.head.new = nv50_head_new,
	.dac = { .nr = 3, .new = nv50_dac_new },
	.sor = { .nr = 2, .new = nv50_sor_new },
	.pior = { .nr = 3, .new = nv50_pior_new },
};

int
nv50_disp_new(struct nvkm_device *device, int index, struct nvkm_disp **pdisp)
{
	return nv50_disp_new_(&nv50_disp, device, index, 2, pdisp);
}
