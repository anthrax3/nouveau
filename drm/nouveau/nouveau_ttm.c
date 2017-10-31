/*
 * Copyright (c) 2007-2008 Tungsten Graphics, Inc., Cedar Park, TX., USA,
 * All Rights Reserved.
 * Copyright (c) 2009 VMware, Inc., Palo Alto, CA., USA,
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "nouveau_drv.h"
#include "nouveau_gem.h"
#include "nouveau_mem.h"
#include "nouveau_ttm.h"

#include <drm/drm_legacy.h>

#include <core/tegra.h>

static int
nouveau_manager_init(struct ttm_mem_type_manager *man, unsigned long psize)
{
	return 0;
}

static int
nouveau_manager_fini(struct ttm_mem_type_manager *man)
{
	return 0;
}

static void
nouveau_manager_del(struct ttm_mem_type_manager *man, struct ttm_mem_reg *reg)
{
	nouveau_mem_del(reg);
}

static void
nouveau_manager_debug(struct ttm_mem_type_manager *man,
		      struct drm_printer *printer)
{
}

static void
nouveau_vram_manager_del(struct ttm_mem_type_manager *man,
			 struct ttm_mem_reg *reg)
{
	struct nvkm_memory *memory = nouveau_mem(reg)->_mem->memory;
	nouveau_mem_del(reg);
	nvkm_memory_unref(&memory);
}

static int
nouveau_vram_manager_new(struct ttm_mem_type_manager *man,
			 struct ttm_buffer_object *bo,
			 const struct ttm_place *place,
			 struct ttm_mem_reg *reg)
{
	struct nouveau_bo *nvbo = nouveau_bo(bo);
	struct nouveau_drm *drm = nvbo->cli->drm;
	struct nouveau_mem *mem;
	int ret;

	if (drm->client.device.info.ram_size == 0)
		return -ENOMEM;

	ret = nouveau_mem_new(&drm->master, nvbo->kind, nvbo->comp, reg);
	mem = nouveau_mem(reg);
	if (ret)
		return ret;

	ret = nouveau_mem_vram(reg, nvbo->contig, nvbo->page);
	if (ret) {
		nouveau_mem_del(reg);
		if (ret == -ENOSPC) {
			reg->mm_node = NULL;
			return 0;
		}
		return ret;
	}

	return 0;
}

const struct ttm_mem_type_manager_func nouveau_vram_manager = {
	.init = nouveau_manager_init,
	.takedown = nouveau_manager_fini,
	.get_node = nouveau_vram_manager_new,
	.put_node = nouveau_vram_manager_del,
	.debug = nouveau_manager_debug,
};

static int
nouveau_gart_manager_new(struct ttm_mem_type_manager *man,
			 struct ttm_buffer_object *bo,
			 const struct ttm_place *place,
			 struct ttm_mem_reg *reg)
{
	struct nouveau_bo *nvbo = nouveau_bo(bo);
	struct nouveau_drm *drm = nvbo->cli->drm;
	struct nouveau_mem *mem;
	int ret;

	ret = nouveau_mem_new(&drm->master, nvbo->kind, nvbo->comp, reg);
	mem = nouveau_mem(reg);
	if (ret)
		return ret;

	mem->_mem = &mem->__mem;
	reg->start = 0;
	return 0;
}

const struct ttm_mem_type_manager_func nouveau_gart_manager = {
	.init = nouveau_manager_init,
	.takedown = nouveau_manager_fini,
	.get_node = nouveau_gart_manager_new,
	.put_node = nouveau_manager_del,
	.debug = nouveau_manager_debug
};

static int
nv04_gart_manager_new(struct ttm_mem_type_manager *man,
		      struct ttm_buffer_object *bo,
		      const struct ttm_place *place,
		      struct ttm_mem_reg *reg)
{
	struct nouveau_bo *nvbo = nouveau_bo(bo);
	struct nouveau_drm *drm = nvbo->cli->drm;
	struct nouveau_mem *mem;
	struct nvkm_mmu *mmu = nvxx_mmu(&drm->client.device);
	int ret;

	ret = nouveau_mem_new(&drm->master, nvbo->kind, nvbo->comp, reg);
	mem = nouveau_mem(reg);
	if (ret)
		return ret;

	ret = nvkm_vm_get(mmu->vmm, reg->num_pages << 12, 12,
			  NV_MEM_ACCESS_RW, &mem->vma[0]);
	if (ret) {
		nouveau_mem_del(reg);
		if (ret == -ENOSPC) {
			reg->mm_node = NULL;
			return 0;
		}
		return ret;
	}

	mem->_mem = &mem->__mem;
	reg->start = mem->vma[0].addr >> PAGE_SHIFT;
	return 0;
}

const struct ttm_mem_type_manager_func nv04_gart_manager = {
	.init = nouveau_manager_init,
	.takedown = nouveau_manager_fini,
	.get_node = nv04_gart_manager_new,
	.put_node = nouveau_manager_del,
	.debug = nouveau_manager_debug
};

int
nouveau_ttm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *file_priv = filp->private_data;
	struct nouveau_drm *drm = nouveau_drm(file_priv->minor->dev);

	if (unlikely(vma->vm_pgoff < DRM_FILE_PAGE_OFFSET))
		return drm_legacy_mmap(filp, vma);

	return ttm_bo_mmap(filp, vma, &drm->ttm.bdev);
}

static int
nouveau_ttm_mem_global_init(struct drm_global_reference *ref)
{
	return ttm_mem_global_init(ref->object);
}

static void
nouveau_ttm_mem_global_release(struct drm_global_reference *ref)
{
	ttm_mem_global_release(ref->object);
}

int
nouveau_ttm_global_init(struct nouveau_drm *drm)
{
	struct drm_global_reference *global_ref;
	int ret;

	global_ref = &drm->ttm.mem_global_ref;
	global_ref->global_type = DRM_GLOBAL_TTM_MEM;
	global_ref->size = sizeof(struct ttm_mem_global);
	global_ref->init = &nouveau_ttm_mem_global_init;
	global_ref->release = &nouveau_ttm_mem_global_release;

	ret = drm_global_item_ref(global_ref);
	if (unlikely(ret != 0)) {
		DRM_ERROR("Failed setting up TTM memory accounting\n");
		drm->ttm.mem_global_ref.release = NULL;
		return ret;
	}

	drm->ttm.bo_global_ref.mem_glob = global_ref->object;
	global_ref = &drm->ttm.bo_global_ref.ref;
	global_ref->global_type = DRM_GLOBAL_TTM_BO;
	global_ref->size = sizeof(struct ttm_bo_global);
	global_ref->init = &ttm_bo_global_init;
	global_ref->release = &ttm_bo_global_release;

	ret = drm_global_item_ref(global_ref);
	if (unlikely(ret != 0)) {
		DRM_ERROR("Failed setting up TTM BO subsystem\n");
		drm_global_item_unref(&drm->ttm.mem_global_ref);
		drm->ttm.mem_global_ref.release = NULL;
		return ret;
	}

	return 0;
}

void
nouveau_ttm_global_release(struct nouveau_drm *drm)
{
	if (drm->ttm.mem_global_ref.release == NULL)
		return;

	drm_global_item_unref(&drm->ttm.bo_global_ref.ref);
	drm_global_item_unref(&drm->ttm.mem_global_ref);
	drm->ttm.mem_global_ref.release = NULL;
}

int
nouveau_ttm_init(struct nouveau_drm *drm)
{
	struct nvkm_device *device = nvxx_device(&drm->client.device);
	struct nvkm_pci *pci = device->pci;
	struct drm_device *dev = drm->dev;
	u8 bits;
	int ret;

	if (pci && pci->agp.bridge) {
		drm->agp.bridge = pci->agp.bridge;
		drm->agp.base = pci->agp.base;
		drm->agp.size = pci->agp.size;
		drm->agp.cma = pci->agp.cma;
	}

	bits = nvxx_mmu(&drm->client.device)->dma_bits;
	if (nvxx_device(&drm->client.device)->func->pci) {
		if (drm->agp.bridge)
			bits = 32;
	} else if (device->func->tegra) {
		struct nvkm_device_tegra *tegra = device->func->tegra(device);

		/*
		 * If the platform can use a IOMMU, then the addressable DMA
		 * space is constrained by the IOMMU bit
		 */
		if (tegra->func->iommu_bit)
			bits = min(bits, tegra->func->iommu_bit);

	}

	ret = dma_set_mask(dev->dev, DMA_BIT_MASK(bits));
	if (ret && bits != 32) {
		bits = 32;
		ret = dma_set_mask(dev->dev, DMA_BIT_MASK(bits));
	}
	if (ret)
		return ret;

	ret = dma_set_coherent_mask(dev->dev, DMA_BIT_MASK(bits));
	if (ret)
		dma_set_coherent_mask(dev->dev, DMA_BIT_MASK(32));

	ret = nouveau_ttm_global_init(drm);
	if (ret)
		return ret;

	ret = ttm_bo_device_init(&drm->ttm.bdev,
				  drm->ttm.bo_global_ref.ref.object,
				  &nouveau_bo_driver,
				  dev->anon_inode->i_mapping,
				  DRM_FILE_PAGE_OFFSET,
				  bits <= 32 ? true : false);
	if (ret) {
		NV_ERROR(drm, "error initialising bo driver, %d\n", ret);
		return ret;
	}

	/* VRAM init */
	drm->gem.vram_available = drm->client.device.info.ram_user;

	arch_io_reserve_memtype_wc(device->func->resource_addr(device, 1),
				   device->func->resource_size(device, 1));

	ret = ttm_bo_init_mm(&drm->ttm.bdev, TTM_PL_VRAM,
			      drm->gem.vram_available >> PAGE_SHIFT);
	if (ret) {
		NV_ERROR(drm, "VRAM mm init failed, %d\n", ret);
		return ret;
	}

	drm->ttm.mtrr = arch_phys_wc_add(device->func->resource_addr(device, 1),
					 device->func->resource_size(device, 1));

	/* GART init */
	if (!drm->agp.bridge) {
		drm->gem.gart_available = nvxx_mmu(&drm->client.device)->limit;
	} else {
		drm->gem.gart_available = drm->agp.size;
	}

	ret = ttm_bo_init_mm(&drm->ttm.bdev, TTM_PL_TT,
			      drm->gem.gart_available >> PAGE_SHIFT);
	if (ret) {
		NV_ERROR(drm, "GART mm init failed, %d\n", ret);
		return ret;
	}

	NV_INFO(drm, "VRAM: %d MiB\n", (u32)(drm->gem.vram_available >> 20));
	NV_INFO(drm, "GART: %d MiB\n", (u32)(drm->gem.gart_available >> 20));
	return 0;
}

void
nouveau_ttm_fini(struct nouveau_drm *drm)
{
	struct nvkm_device *device = nvxx_device(&drm->client.device);

	ttm_bo_clean_mm(&drm->ttm.bdev, TTM_PL_VRAM);
	ttm_bo_clean_mm(&drm->ttm.bdev, TTM_PL_TT);

	ttm_bo_device_release(&drm->ttm.bdev);

	nouveau_ttm_global_release(drm);

	arch_phys_wc_del(drm->ttm.mtrr);
	drm->ttm.mtrr = 0;
	arch_io_free_memtype_wc(device->func->resource_addr(device, 1),
				device->func->resource_size(device, 1));

}
