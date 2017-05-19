#ifndef __NVKM_DISP_OUTP_H__
#define __NVKM_DISP_OUTP_H__
#include <engine/disp.h>

#include <subdev/bios.h>
#include <subdev/bios/dcb.h>

struct nvkm_outp {
	const struct nvkm_outp_func *func;
	struct nvkm_disp *disp;
	int index;
	struct dcb_output info;

	// whatever (if anything) is pointed at by the dcb device entry
	struct nvkm_i2c_bus *i2c;
	int or;

	struct list_head head;
	struct nvkm_conn *conn;
};

void nvkm_outp_ctor(const struct nvkm_outp_func *, struct nvkm_disp *,
		    int index, struct dcb_output *, struct nvkm_outp *);
void nvkm_outp_del(struct nvkm_outp **);
void nvkm_outp_init(struct nvkm_outp *);
void nvkm_outp_fini(struct nvkm_outp *);

struct nvkm_outp_func {
	void *(*dtor)(struct nvkm_outp *);
	void (*init)(struct nvkm_outp *);
	void (*fini)(struct nvkm_outp *);
};

#define nvkm_output nvkm_outp
#define nvkm_output_func nvkm_outp_func
#define nvkm_output_new_ nvkm_outp_new_

int nvkm_outp_new_(const struct nvkm_outp_func *, struct nvkm_disp *,
		   int index, struct dcb_output *, struct nvkm_output **);

int nv50_dac_output_new(struct nvkm_disp *, int, struct dcb_output *,
			struct nvkm_output **);
int nv50_sor_output_new(struct nvkm_disp *, int, struct dcb_output *,
			struct nvkm_output **);
int nv50_pior_output_new(struct nvkm_disp *, int, struct dcb_output *,
			 struct nvkm_output **);

u32 g94_sor_dp_lane_map(struct nvkm_device *, u8 lane);

void gm200_sor_magic(struct nvkm_output *outp);

#define OUTP_MSG(o,l,f,a...) do {                                              \
	struct nvkm_outp *_outp = (o);                                         \
	nvkm_##l(&_outp->disp->engine.subdev, "outp %02x:%04x:%04x: "f"\n",    \
		 _outp->index, _outp->info.hasht, _outp->info.hashm, ##a);     \
} while(0)
#define OUTP_ERR(o,f,a...) OUTP_MSG((o), error, f, ##a)
#define OUTP_DBG(o,f,a...) OUTP_MSG((o), debug, f, ##a)
#define OUTP_TRACE(o,f,a...) OUTP_MSG((o), trace, f, ##a)
#endif
