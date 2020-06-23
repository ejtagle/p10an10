/* Gralloc wrapper to make it fully compatible with JB */

#define LOG_TAG "gralloc"
#define LOG_NDEBUG 0

#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include <cutils/log.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include "gralloc_priv.h"

struct g_gralloc_context_t {
    alloc_device_t  device;
};

struct gralloc_context_t {
    alloc_device_t  device;
	
	/* our private data here */
	g_gralloc_context_t *g_ctx;
};

struct g_fb_context_t {
    framebuffer_device_t  device;
};

struct fb_context_t {
    framebuffer_device_t  device;
	
	/* our private data here */
	g_fb_context_t *g_ctx;
};


// Get the original gralloc
static private_module_t* get_gralloc(void)
{
	/* The original library implementation */
	static private_module_t* org_gralloc = {NULL};

	if (!org_gralloc) {
		void* handle;
		ALOGI("Resolving original HWC module...");
		
		handle = dlopen("/system/lib/hw/gralloc.tegra_v0.so",RTLD_LAZY);
		if(!handle) {
			ALOGE("Unable to load original gralloc module");
			return NULL;
		}

		/* Get a pointer to the original gralloc */
		org_gralloc = (private_module_t*) dlsym(handle,HAL_MODULE_INFO_SYM_AS_STR);
		if (!org_gralloc) {
			ALOGE("Unable to resolve GRALLOC HMI");
			dlclose(handle);
			handle = NULL;
			return NULL;
		}
		
		/* Fill all the defined by Nvidia fields with a copy of the original data */
		HAL_MODULE_INFO_SYM.base.perform = org_gralloc->base.perform; /* Copy the fn pointer, as this is the only way to support variable args (nvidia does not use it)*/
		memcpy(HAL_MODULE_INFO_SYM.reserved_proc, org_gralloc->reserved_proc, sizeof(org_gralloc->reserved_proc));
		
		/* Copy nvidia proprietary fields */
		memcpy((char*)(HAL_MODULE_INFO_SYM.base) + sizeof(gralloc_module_t) , (char*)(org_gralloc->base) + sizeof(gralloc_module_t), sizeof(private_module_t) - sizeof(gralloc_module_t) );
		
		ALOGI("Loaded the original GRALLOC module");
	}
	
	return org_gralloc;
}

static int gralloc_alloc(alloc_device_t* dev,
        int w, int h, int format, int usage,
        buffer_handle_t* pHandle, int* pStride)
{
    LOGV("%s: ", __func__);
	
    gralloc_context_t* ctx = reinterpret_cast<gralloc_context_t*>(dev);
    return ctx->g_ctx->device.alloc(dev, w, h, format, usage, pHandle, pStride);
}

static int gralloc_free(alloc_device_t* dev,
        buffer_handle_t handle)
{
    LOGV("%s: ", __func__);
	
    gralloc_context_t* ctx = reinterpret_cast<gralloc_context_t*>(dev);
    return ctx->g_ctx->device.free(dev, handle);
}

static int gralloc_dump(alloc_device_t* dev, char* buff, int buff_len)
{
	LOGV("%s: ", __func__);
	
	gralloc_context_t* ctx = reinterpret_cast<gralloc_context_t*>(dev);
	return ctx->g_ctx->device.dump(dev, buff, buff_len);
}

static int gralloc_close(struct hw_device_t *dev)
{
    LOGV("%s: ", __func__);
	
    gralloc_context_t* ctx = reinterpret_cast<gralloc_context_t*>(dev);
    if (ctx) {
        if (ctx->g_ctx) {
            ctx->g_ctx->device.common.close(&ctx->g_ctx->device.common);
        }
        free(ctx);
    }
    return 0;
}

static int fb_setSwapInterval(struct framebuffer_device_t* dev,
            int interval)
{
    LOGV("%s: interval = %d", __func__, interval);
	
    fb_context_t* ctx = reinterpret_cast<fb_context_t*>(dev);
    return ctx->g_ctx->device.setSwapInterval(&ctx->g_ctx->device, interval);
}

static int fb_setUpdateRect (struct framebuffer_device_t* dev, int left, int top, int width, int height)
{
    LOGV("%s", __func__);
	
    fb_context_t* ctx = reinterpret_cast<fb_context_t*>(dev);
    return ctx->g_ctx->device.setUpdateRect(&ctx->g_ctx->device, left, top, width, height);
}

static int fb_post(struct framebuffer_device_t* dev, buffer_handle_t buffer)
{
    LOGV("%s: buffer = %p", __func__, buffer);
	
    fb_context_t* ctx = reinterpret_cast<fb_context_t*>(dev);
    return ctx->g_ctx->device.post(&ctx->g_ctx->device, buffer);
}

static int fb_compositionComplete(struct framebuffer_device_t* dev)
{
    LOGV("%s: ", __func__);
    fb_context_t* ctx = reinterpret_cast<fb_context_t*>(dev);
    return ctx->g_ctx->device.compositionComplete(&ctx->g_ctx->device);
}

static int fb_dump(framebuffer_device_t* dev, char* buff, int buff_len)
{
	LOGV("%s: ", __func__);
	
	fb_context_t* ctx = reinterpret_cast<fb_context_t*>(dev);
	return ctx->g_ctx->device.dump(dev, buff, buff_len);
}

static int fb_enableScreen(framebuffer_device_t* dev, int enable)
{
	LOGV("%s: enable:%d", __func__, enable);
	
	fb_context_t* ctx = reinterpret_cast<fb_context_t*>(dev);
	
	if (!ctx->g_ctx->device.enableScreen) {
		ALOGD("Unsupported enableScreen. Emulating it");
		return 0;
	}
	
	return ctx->g_ctx->device.enableScreen(dev, enable);
}

static int fb_close(struct hw_device_t *dev)
{
    LOGV("%s: ", __func__);
	
    fb_context_t* ctx = reinterpret_cast<fb_context_t*>(dev);
    if (ctx) {
        if (ctx->g_ctx) {
            ctx->g_ctx->device.common.close(&ctx->g_ctx->device.common);
        }
        free(ctx);
    }
    return 0;
}

static int gralloc_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    LOGV("%s: %s", __func__, name);
    int status = -EINVAL;
    
    private_module_t* org_gralloc = get_gralloc();
    
    status = org_gralloc->base.common.methods->open(module, name, device);
	if (status != 0)
		return status;
		
    if (!strcmp(name, GRALLOC_HARDWARE_GPU0)) {
	
        gralloc_context_t *dev = (gralloc_context_t*)malloc(sizeof(*dev));
        
        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));
        
        dev->g_ctx = reinterpret_cast<g_gralloc_context_t*>(*device);
        
        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = gralloc_close;

        dev->device.alloc   = gralloc_alloc;
        dev->device.free    = gralloc_free;
		if (dev->g_ctx->device.dump) dev->device.dump = gralloc_dump;
		memcpy(dev->device.reserved_proc, dev->g_ctx->device.reserved_proc, sizeof(dev->device.reserved_proc));

        *device = &dev->device.common;
		
    } else {
        fb_context_t *dev;
        dev = (fb_context_t*)malloc(sizeof(*dev));
        
        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));
        
        dev->g_ctx = reinterpret_cast<g_fb_context_t*>(*device);
        
        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = fb_close;

		const_cast<uint32_t&>(dev->device.flags)        = dev->g_ctx->device.flags;
		const_cast<uint32_t&>(dev->device.width)        = dev->g_ctx->device.width;
		const_cast<uint32_t&>(dev->device.height)       = dev->g_ctx->device.height;
		const_cast<int&>(dev->device.stride)            = dev->g_ctx->device.stride;
		const_cast<int&>(dev->device.format)            = dev->g_ctx->device.format;
		const_cast<float&>(dev->device.xdpi)            = dev->g_ctx->device.xdpi;
		const_cast<float&>(dev->device.ydpi)            = dev->g_ctx->device.ydpi;
		const_cast<float&>(dev->device.fps)             = dev->g_ctx->device.fps;
		const_cast<int&>(dev->device.minSwapInterval)   = dev->g_ctx->device.minSwapInterval;
		const_cast<int&>(dev->device.maxSwapInterval)   = dev->g_ctx->device.maxSwapInterval;
		const_cast<int&>(dev->device.numFramebuffers)   = dev->g_ctx->device.numFramebuffers;
		memcpy(dev->device.reserved, dev->g_ctx->device.reserved, sizeof(dev->device.reserved));
		
        if (dev->g_ctx->device.setSwapInterval) dev->device.setSwapInterval = fb_setSwapInterval;
		if (dev->g_ctx->device.setUpdateRect) dev->device.setUpdateRect = fb_setUpdateRect;
        dev->device.post = fb_post;
        if (dev->g_ctx->device.compositionComplete) dev->device.compositionComplete = fb_compositionComplete;
		if (dev->g_ctx->device.dump) dev->device.dump = fb_dump;
		dev->device.enableScreen = fb_enableScreen;
		memcpy(dev->device.reserved_proc, dev->g_ctx->device.reserved_proc, sizeof(dev->device.reserved_proc));
        
		/* Fix up older version structs to report proper data */
		if (dev->device.numFramebuffers == 0) { 
			dev->device.numFramebuffers = 1;
		}
		
        *device = &dev->device.common;
    }
    
    return status;
}

static int gralloc_register_buffer(gralloc_module_t const* module,
        buffer_handle_t handle)
{
    LOGV("%s: ", __func__);
	
    private_module_t* org_gralloc = get_gralloc();
    return org_gralloc->base.registerBuffer(module, handle);
}

static int gralloc_unregister_buffer(gralloc_module_t const* module,
        buffer_handle_t handle)
{
    LOGV("%s: ", __func__);
	
    private_module_t* org_gralloc = get_gralloc();
    return org_gralloc->base.unregisterBuffer(module, handle);
}

static int gralloc_lock(gralloc_module_t const* module,
        buffer_handle_t handle, int usage,
        int l, int t, int w, int h,
        void** vaddr)
{
    LOGV("%s: ", __func__);
	
    private_module_t* org_gralloc = get_gralloc();
    return org_gralloc->base.lock(module, handle, usage, l, t, w, h, vaddr);
}

static int gralloc_unlock(gralloc_module_t const* module, 
        buffer_handle_t handle)
{
    LOGV("%s: ", __func__);
	
	private_module_t* org_gralloc = get_gralloc();
    return org_gralloc->base.unlock(module, handle);
}

static struct hw_module_methods_t gralloc_module_methods = {
        open: gralloc_device_open
};

struct private_module_t HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
            version_major: 1,
            version_minor: 0,
            id: GRALLOC_HARDWARE_MODULE_ID,
            name: "Graphics Memory Allocator Module",
            author: "The Android Open Source Project",
            methods: &gralloc_module_methods
        },
        registerBuffer: gralloc_register_buffer,
        unregisterBuffer: gralloc_unregister_buffer,
        lock: gralloc_lock,
        unlock: gralloc_unlock,
        perform: NULL,
    },
};

