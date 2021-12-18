#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef __SCALER_H_
#define __SCALER_H_

#define MASK_RGB   0x00FFFFFF
#define MASK_ALPHA 0xFF000000
#define MASK_1  0x00FF0000
#define MASK_2  0x0000FF00
#define MASK_3  0x000000FF
#define MASK_13 0x00FF00FF
#define trY   0x00300000
#define trU   0x00000700
#define trV   0x00000006

#define ALPHA(rgb) ((rgb >> 24) & 0xff)
#define RED(rgb)   ((rgb >> 16) & 0xff)
#define GREEN(rgb) ((rgb >> 8) & 0xff)
#define BLUE(rgb)  (rgb & 0xff)

#ifdef __cplusplus
extern "C" {
#endif

// XBR scaler
    void xbr_filter( uint32_t *src, uint32_t *dst, int inWidth, int inHeight, int scaleFactor);

// HQX scaler
    void hqx(uint32_t * sp,  uint32_t * dp, int Xres, int Yres, int scalefactor);

// ScaleX scaler
    void scaler_scalex(uint32_t * sp,  uint32_t * dp, int Xres, int Yres, int scalefactor);

// Eagle scaler
    void scaler_eagle(uint32_t * sp,  uint32_t * dp, int Xres, int Yres, int scalefactor);

#ifdef __cplusplus
}
#endif

#endif //__SCALER_H_//
