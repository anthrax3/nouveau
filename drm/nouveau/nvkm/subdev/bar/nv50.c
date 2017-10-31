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

#include <core/gpuobj.h>
#include <subdev/fb.h>
#include <subdev/mmu.h>
#include <subdev/timer.h>

struct nvkm_vm *
nv50_bar_kmap(struct nvkm_bar *base)
{
	return nv50_bar(base)->bar2_vm;
}

int
nv50_bar_umap(struct nvkm_bar *base, u64 size, int type, struct nvkm_vma *vma)
{
	struct nv50_bar *bar = nv50_bar(base);
	return nvkm_vm_get(bar->bar1_vm, size, type, NV_MEM_ACCESS_RW, vma);
}

static void
nv50_bar_flush(struct nvkm_bar *base)
{
	struct nv50_bar *bar = nv50_bar(base);
	struct nvkm_device *device = bar->base.subdev.device;
	unsigned long flags;
	spin_lock_irqsave(&bar->base.lock, flags);
	nvkm_wr32(device, 0x00330c, 0x00000001);
	nvkm_msec(device, 2000,
		if (!(nvkm_rd32(device, 0x00330c) & 0x00000002))
			break;
	);
	spin_unlock_irqrestore(&bar->base.lock, flags);
}

void
nv50_bar_bar1_wait(struct nvkm_bar *base)
{
	nvkm_bar_flush(base);
}

void
nv50_bar_bar1_fini(struct nvkm_bar *bar)
{
	nvkm_wr32(bar->subdev.device, 0x001708, 0x00000000);
}

void
nv50_bar_bar1_init(struct nvkm_bar *base)
{
	struct nvkm_device *device = base->subdev.device;
	struct nv50_bar *bar = nv50_bar(base);
	nvkm_wr32(device, 0x001708, 0x80000000 | bar->bar1->node->offset >> 4);
}

void
nv50_bar_bar2_fini(struct nvkm_bar *bar)
{
	nvkm_wr32(bar->subdev.device, 0x00170c, 0x00000000);
}

void
nv50_bar_bar2_init(struct nvkm_bar *base)
{
	struct nvkm_device *device = base->subdev.device;
	struct nv50_bar *bar = nv50_bar(base);
	nvkm_wr32(device, 0x001704, 0x00000000 | bar->mem->addr >> 12);
	nvkm_wr32(device, 0x001704, 0x40000000 | bar->mem->addr >> 12);
	nvkm_wr32(device, 0x00170c, 0x80000000 | bar->bar2->node->offset >> 4);
}

void
nv50_bar_init(struct nvkm_bar *base)
{
	struct nv50_bar *bar = nv50_bar(base);
	struct nvkm_device *device = bar->base.subdev.device;
	int i;

	for (i = 0; i < 8; i++)
		nvkm_wr32(device, 0x001900 + (i * 4), 0x00000000);
}

int
nv50_bar_oneinit(struct nvkm_bar *base)
{
	struct nv50_bar *bar = nv50_bar(base);
	struct nvkm_device *device = bar->base.subdev.device;
	static struct lock_class_key bar1_lock;
	static struct lock_class_key bar2_lock;
	struct nvkm_vm *vm;
	u64 start, limit;
	int ret;

	ret = nvkm_gpuobj_new(device, 0x20000, 0, false, NULL, &bar->mem);
	if (ret)
		return ret;

	ret = nvkm_gpuobj_new(device, bar->pgd_addr, 0, false, bar->mem,
			      &bar->pad);
	if (ret)
		return ret;

	ret = nvkm_gpuobj_new(device, 0x4000, 0, false, bar->mem, &bar->pgd);
	if (ret)
		return ret;

	/* BAR2 */
	start = 0x0100000000ULL;
	limit = start + device->func->resource_size(device, 3);

	ret = nvkm_vm_new(device, start, limit - start, start, &bar2_lock, &vm);
	if (ret)
		return ret;

	atomic_inc(&vm->engref[NVKM_SUBDEV_BAR]);

	ret = nvkm_vm_boot(vm, limit-- - start);
	if (ret)
		return ret;

	ret = nvkm_vm_ref(vm, &bar->bar2_vm, bar->pgd);
	nvkm_vm_ref(NULL, &vm, NULL);
	if (ret)
		return ret;

	ret = nvkm_gpuobj_new(device, 24, 16, false, bar->mem, &bar->bar2);
	if (ret)
		return ret;

	nvkm_kmap(bar->bar2);
	nvkm_wo32(bar->bar2, 0x00, 0x7fc00000);
	nvkm_wo32(bar->bar2, 0x04, lower_32_bits(limit));
	nvkm_wo32(bar->bar2, 0x08, lower_32_bits(start));
	nvkm_wo32(bar->bar2, 0x0c, upper_32_bits(limit) << 24 |
				   upper_32_bits(start));
	nvkm_wo32(bar->bar2, 0x10, 0x00000000);
	nvkm_wo32(bar->bar2, 0x14, 0x00000000);
	nvkm_done(bar->bar2);

	/* BAR1 */
	start = 0x0000000000ULL;
	limit = start + device->func->resource_size(device, 1);

	ret = nvkm_vm_new(device, start, limit-- - start, start, &bar1_lock, &vm);
	if (ret)
		return ret;

	atomic_inc(&vm->engref[NVKM_SUBDEV_BAR]);

	ret = nvkm_vm_ref(vm, &bar->bar1_vm, bar->pgd);
	nvkm_vm_ref(NULL, &vm, NULL);
	if (ret)
		return ret;

	ret = nvkm_gpuobj_new(device, 24, 16, false, bar->mem, &bar->bar1);
	if (ret)
		return ret;

	nvkm_kmap(bar->bar1);
	nvkm_wo32(bar->bar1, 0x00, 0x7fc00000);
	nvkm_wo32(bar->bar1, 0x04, lower_32_bits(limit));
	nvkm_wo32(bar->bar1, 0x08, lower_32_bits(start));
	nvkm_wo32(bar->bar1, 0x0c, upper_32_bits(limit) << 24 |
				   upper_32_bits(start));
	nvkm_wo32(bar->bar1, 0x10, 0x00000000);
	nvkm_wo32(bar->bar1, 0x14, 0x00000000);
	nvkm_done(bar->bar1);
	return 0;
}

void *
nv50_bar_dtor(struct nvkm_bar *base)
{
	struct nv50_bar *bar = nv50_bar(base);
	nvkm_gpuobj_del(&bar->bar1);
	nvkm_vm_ref(NULL, &bar->bar1_vm, bar->pgd);
	nvkm_gpuobj_del(&bar->bar2);
	if (bar->bar2_vm) {
		nvkm_memory_del(&bar->bar2_vm->pgt[0].mem[0]);
		nvkm_vm_ref(NULL, &bar->bar2_vm, bar->pgd);
	}
	nvkm_gpuobj_del(&bar->pgd);
	nvkm_gpuobj_del(&bar->pad);
	nvkm_gpuobj_del(&bar->mem);
	return bar;
}

int
nv50_bar_new_(const struct nvkm_bar_func *func, struct nvkm_device *device,
	      int index, u32 pgd_addr, struct nvkm_bar **pbar)
{
	struct nv50_bar *bar;
	if (!(bar = kzalloc(sizeof(*bar), GFP_KERNEL)))
		return -ENOMEM;
	nvkm_bar_ctor(func, device, index, &bar->base);
	bar->pgd_addr = pgd_addr;
	*pbar = &bar->base;
	return 0;
}

static const struct nvkm_bar_func
nv50_bar_func = {
	.dtor = nv50_bar_dtor,
	.oneinit = nv50_bar_oneinit,
	.init = nv50_bar_init,
	.bar1.init = nv50_bar_bar1_init,
	.bar1.fini = nv50_bar_bar1_fini,
	.bar1.wait = nv50_bar_bar1_wait,
	.bar2.init = nv50_bar_bar2_init,
	.bar2.fini = nv50_bar_bar2_fini,
	.bar2.wait = nv50_bar_bar1_wait,
	.kmap = nv50_bar_kmap,
	.umap = nv50_bar_umap,
	.flush = nv50_bar_flush,
};

int
nv50_bar_new(struct nvkm_device *device, int index, struct nvkm_bar **pbar)
{
	return nv50_bar_new_(&nv50_bar_func, device, index, 0x1400, pbar);
}
