#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define  LOG_TAG  "fbmirror"

#include <cutils/log.h>  

#ifndef min
#define min(x,y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef max
#define max(x,y) (((x) > (y)) ? (x) : (y))
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct ccinfo {
	int bits;
	int off;
} ccinfo_t;

typedef struct fbhandle {
	int handle;
	int xres,yres;
	int stride;		// In bytes
	int bpp;		// Bits per pizel
	ccinfo_t red;
	ccinfo_t green;
	ccinfo_t blue;
	void* pixels;
	int screensize;
} fb_t;

int open_fb(fb_t* ctx,int idx)
{
	char buf[32];
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	int screensize;
	
	sprintf(buf,"/dev/fb%d",idx);	
	ctx->handle = open(buf, O_RDWR);
	if (ctx->handle < 0) {
	
		sprintf(buf,"/dev/graphics/fb%d",idx);	
		ctx->handle = open(buf, O_RDWR);
	}
	if (ctx->handle < 0) {
		ALOGE("Unable to open '%s'\n",buf);
		return 0;
	}
	ALOGI("Successfully opened '%s'\n",buf);
	
	/* Get fixed screen information */
	if (ioctl(ctx->handle, FBIOGET_FSCREENINFO, &finfo)) {
		ALOGE("Error reading fixed information.\n");
		close(ctx->handle);
		return 0;
	}
	
	/* Get variable screen information */
	if (ioctl(ctx->handle, FBIOGET_VSCREENINFO, &vinfo)) {
		ALOGE("Error reading variable information.\n");
		close(ctx->handle);
		return 0;
	}

	/* Print some screen info currently available */
  	ALOGD("Screen resolution: (%dx%d)\n", vinfo.xres,vinfo.yres);
  	ALOGD("Line width in bytes %d\n", finfo.line_length);
  	ALOGD("bits per pixel : %d\n", vinfo.bits_per_pixel);
  	ALOGD("Red: length %d bits, offset %d\n", vinfo.red.length, vinfo.red.offset);
  	ALOGD("Green: length %d bits, offset %d\n", vinfo.green.length, vinfo.green.offset);
  	ALOGD("Blue: length %d bits, offset %d\n", vinfo.blue.length, vinfo.blue.offset);

	ctx->xres = vinfo.xres;
	ctx->yres = vinfo.yres;
	ctx->stride = finfo.line_length;
	ctx->bpp = vinfo.bits_per_pixel;
	ctx->red.off = vinfo.red.offset;
	ctx->red.bits = vinfo.red.length;
	ctx->green.off = vinfo.green.offset;
	ctx->green.bits = vinfo.green.length;
	ctx->blue.off = vinfo.blue.offset;
	ctx->blue.bits = vinfo.blue.length;
	
  	/* Calculate the size to mmap */
  	screensize = finfo.line_length * vinfo.yres;
	ctx->screensize = screensize;
	
	// Map the device to memory
	ctx->pixels = (void *)mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->handle, 0);
	if (ctx->pixels == MAP_FAILED) {
		ctx->pixels = (void *)mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_PRIVATE, ctx->handle, 0);
		if (ctx->handle == MAP_FAILED) {	
			ALOGE("failed to map framebuffer device to memory.\n");
			close(ctx->handle);
			return 0;
		}
	}
	
	ALOGI("The framebuffer '%s' was mapped to memory successfully at %p.\n",buf,ctx->pixels);
	return 1;
}

int close_fb(fb_t* ctx)
{
	if (ctx->handle < 0)
		return 1;
		
	munmap(ctx->pixels, ctx->screensize);
	close(ctx->handle);
	ctx->handle = -1;
	
	ALOGI("Framebuffer closed successfully\n");
	return 1;
}

#define DLFB_IOCTL_REPORT_DAMAGE 0xAA
struct dloarea {
	int x, y;
	int w, h;
	int x2, y2;
};

int transfer_fb(fb_t* src,fb_t* dst)
{
	struct dloarea area;

	// Minimum transfer rectangle
	int xres = min( src->xres, dst->xres );
	int yres = min( src->yres, dst->yres );
	
	// Source and destination pixel pointers centered to copy rectangles
	void* srcp = (char*)src->pixels + (( ((src->xres - xres) >> 1 ) * src->bpp ) >> 3) + (((src->yres - yres) >> 1 ) * src->stride);
	void* dstp = (char*)dst->pixels + (( ((dst->xres - xres) >> 1 ) * dst->bpp ) >> 3) + (((dst->yres - yres) >> 1 ) * dst->stride);

	// Calculate active bytes
	int srcab = (xres * src->bpp) >> 3; 
	int dstab = (xres * dst->bpp) >> 3; 
	
	// Calculate delta strides
	int srcds = src->stride - ((src->xres * src->bpp) >> 3); 
	int dstds = dst->stride - ((dst->xres * dst->bpp) >> 3); 

	// Calculate the damage area
	area.w = xres;
	area.h = yres;
	area.x = (dst->xres - xres) >> 1;
	area.y = (dst->yres - yres) >> 1;
	
	switch (src->bpp) {
	case 32:
		switch (dst->bpp) {
		case 32:
			{ //32->32
				// Trivial copy
				int y = yres;
				while (y--) {
					memcpy(dstp,srcp,srcab);
					srcp = (char*) srcp + src->stride;
					dstp = (char*) dstp + dst->stride;
				};
			}
			return ioctl(dst->handle, DLFB_IOCTL_REPORT_DAMAGE, &area) >= 0;
		case 24:
			{ //32->24
				// Most usual case...
				int y = yres;
				u32* psrc = srcp;
				u8* pdst = dstp;
				srcds >>= 2;
				while (y--) {
					int x = xres;
					while (x--) {
						u32 col = *psrc++;
						pdst[0] =  col         & 0xFFU;
						pdst[1] = (col >> 8U)  & 0xFFU;
						pdst[2] = (col >> 16U) & 0xFFU;
						pdst += 3;
					};
					psrc += srcds;
					pdst += dstds;
				};
			}
			return ioctl(dst->handle, DLFB_IOCTL_REPORT_DAMAGE, &area) >= 0;
		case 16:
			{ //32->16
				// Most usual case...
				int y = yres;
				u32* psrc = srcp;
				u16* pdst = dstp;
				srcds >>= 2;
				dstds >>= 1;
				while (y--) {
					int x = xres;
					while (x--) {
						u32 col = *psrc++;
						
						// 32bit: aaaaaaaarrrrrrrrggggggggbbbbbbbb
						// 16bit:                 rrrrrggggggbbbbb
						*pdst++ =
							((col >> 3U) & 0x001FU) |
							((col >> 5U) & 0x07E0U) |
							((col >> 8U) & 0xF100U);
					};
					psrc += srcds;
					pdst += dstds;
				};
			}
			return ioctl(dst->handle, DLFB_IOCTL_REPORT_DAMAGE, &area) >= 0;
		}
	case 24:
		switch (dst->bpp) {
		case 32:
			{ //24->32
				// Most usual case...
				int y = yres;
				u8* psrc = srcp;
				u32* pdst = dstp;
				dstds >>= 2;
				while (y--) {
					int x = xres;
					while (x--) {
						u32 col = psrc[0] | (psrc[1] << 8U) | (psrc[2] << 16U);
						psrc += 3;
						*pdst++ = col;
					};
					psrc += srcds;
					pdst += dstds;
				};
			}
			return ioctl(dst->handle, DLFB_IOCTL_REPORT_DAMAGE, &area) >= 0;
		case 24:
			{ //24->24
				// Trivial copy
				int y = yres;
				while (y--) {
					memcpy(dstp,srcp,srcab);
					srcp = (char*) srcp + src->stride;
					dstp = (char*) dstp + dst->stride;
				};
			}
			return ioctl(dst->handle, DLFB_IOCTL_REPORT_DAMAGE, &area) >= 0;
		case 16:
			{ //24->16
				// Most usual case...
				int y = yres;
				u8* psrc = srcp;
				u16* pdst = dstp;
				dstds >>= 1;
				while (y--) {
					int x = xres;
					while (x--) {
						u32 col = psrc[0] | (psrc[1] << 8U) | (psrc[2] << 16U);
						psrc += 3;
						
						// 24bit: rrrrrrrrggggggggbbbbbbbb
						// 16bit:         rrrrrggggggbbbbb
						*pdst++ =
							((col >> 3U) & 0x001FU) |
							((col >> 5U) & 0x07E0U) |
							((col >> 8U) & 0xF100U);
					};
					psrc += srcds;
					pdst += dstds;
				};
			}
			return ioctl(dst->handle, DLFB_IOCTL_REPORT_DAMAGE, &area) >= 0;
		}
	case 16:
		switch (dst->bpp) {
		case 32:
			{ //16->32
				// Most usual case...
				int y = yres;
				u16* psrc = srcp;
				u32* pdst = dstp;
				srcds >>= 1;
				dstds >>= 2;
				while (y--) {
					int x = xres;
					while (x--) {
						u32 col = *psrc++;
						// 16bit:                 rrrrrggggggbbbbb
						// 32bit: aaaaaaaarrrrrrrrggggggggbbbbbbbb
						*pdst++ =
							((col << 3U) & 0x0000F1U) |
							((col << 5U) & 0x00FC00U) |
							((col << 8U) & 0xF10000U);
					};
					psrc += srcds;
					pdst += dstds;
				};
			}
			return ioctl(dst->handle, DLFB_IOCTL_REPORT_DAMAGE, &area) >= 0;
		case 24:
			{ //16->24
				// Most usual case...
				int y = yres;
				u16* psrc = srcp;
				u8* pdst = dstp;
				srcds >>= 1;
				while (y--) {
					int x = xres;
					while (x--) {
						u32 col = *psrc++;
						// 16bit:                 rrrrrggggggbbbbb
						// 32bit: aaaaaaaarrrrrrrrggggggggbbbbbbbb
						col =
							((col << 3U) & 0x0000F1U) |
							((col << 5U) & 0x00FC00U) |
							((col << 8U) & 0xF10000U);
						pdst[0] =  col         & 0xFFU;
						pdst[1] = (col >> 8U)  & 0xFFU;
						pdst[2] = (col >> 16U) & 0xFFU;
						pdst += 3;
					};
					psrc += srcds;
					pdst += dstds;
				};
			}
			return ioctl(dst->handle, DLFB_IOCTL_REPORT_DAMAGE, &area) >= 0;
		case 16:
			{
				// Trivial copy
				int y = yres;
				while (y--) {
					memcpy(dstp,srcp,srcab);
					srcp = (char*) srcp + src->stride;
					dstp = (char*) dstp + dst->stride;
				};
			}
			return ioctl(dst->handle, DLFB_IOCTL_REPORT_DAMAGE, &area) >= 0;
		}
	}
	
	ALOGE("Unsupported transfer:  src bpp: %d, dst bpp: %d\n",src->bpp,dst->bpp);
	return 0;
}

int main(void)
{
	fb_t src,dst;
	ALOGI("Starting DisplayLink mirroring service...\n");
	
waitit:
	/* Wait until a DisplayLink framebuffer appears */
	
	ALOGD("Trying to open a DisplayLink framebuffer\n");
	while (!open_fb(&dst,2)) {
		sleep(1);
	};

	ALOGI("DisplayLink adapter plugged in!\n");

	ALOGD("Trying to open main framebuffer\n");
	if (!open_fb(&src,0)) {
		ALOGE("Failed to open main framebuffer\n");
		close_fb(&dst);
		goto waitit;
	}
	
	ALOGI("Main framebuffer opened -- Starting mirroring operation\n");
	while(1) {
		/* Copy from one to the other */
		if (!transfer_fb(&src,&dst)) {
			ALOGE("Failed to transfer... Assume DuisplayLink adapter was removed\n");
			close_fb(&dst);
			close_fb(&src);
			goto waitit;
		}
		
		/* Limit rate to 24 fps... */
		usleep(40*1000);
	}

	/* not reached */
	return 0;
}


