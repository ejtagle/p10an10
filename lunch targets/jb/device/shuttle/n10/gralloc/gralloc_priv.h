
#ifndef GRALLOC_PRIV_H_
#define GRALLOC_PRIV_H_

#include <stdint.h>
#include <limits.h>
#include <sys/cdefs.h>
#include <hardware/gralloc.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include <cutils/native_handle.h>

struct private_handle_t;

struct t_private_module_t {
    gralloc_module_t base;
    
    struct private_handle_t* framebuffer;
    uint32_t fbFormat;
    uint32_t flags;
    uint32_t numBuffers;
    uint32_t bufferMask;
};

struct private_module_t {
    gralloc_module_t base;

	/* NVidia has several pointers to functions here that seem to be used by the hwcomposer library: Make sure to copy them */
	void (*NvGrAddFence)(void);
	void (*NvGrGetFences)(void);
	void* unk1;
	void (*NvGrFbSetOverlays)(void);
	void (*NvGrFbSetComposite)(void);
	void (*NvGrSetStereoInfo)(void);
	void (*NvGrSetSourceCrop)(void);
	void (*NvGrFbHdmiStatus)(void);
	void (*NvGrGetDisplayMode)(void);
	void (*NvGrListDisplayModes)(void);
	void (*NvGrScratchAssign)(void);
	void (*NvGrScratchFrameStart)(void);
	void (*NvGrScratchFrameEnd)(void);
	void (*NvGrAllocInternal)(void);
	void (*NvGrFreeInternal)(void);
	void (*NvGr2dCopyBuffer)(void);
	void (*NvGr2dCopyEdges)(void);
	void (*NvGr2dBlit)(void);
	void (*NvGr2DClear)(void);
	void (*NvGrSetRedrawCallback)(void);
	void (*NvGrWfdEnable)(void);
	void (*NvGrGetPostSyncFence)(void);
	int val0x4000;
	int unk2[100];
};

#define DL_HMI_SIZE 404
#define DL_HMI_USED 76
#define DL_HMI_LEFT DL_HMI_SIZE-DL_HMI_USED


#ifdef __cplusplus
struct private_handle_t : public native_handle {
#else
struct private_handle_t {
    struct native_handle nativeHandle;
#endif

    // file-descriptors
    int     fd;
    // ints
    int     magic;
    int     flags;
    int     size;
    int     offset;

    // FIXME: the attributes below should be out-of-line
    int     base;
    int     pid;
};

#endif