/*
 * Author: 2016 zvezdochiot (mykaralw@yandex.ru)
 *
 * Public Domain Mark 1.0
 * No Copyright
 *
 *   This work has been identified as being free of known restrictions
 * under copyright law, including all related and neighboring rights.
 *
 *
 *   You can copy, modify, distribute and perform the work, even for
 * commercial purposes, all without asking permission. See Other
 * Information below.
 *
 * Other Information
 *
 *   The work may not be free of known copyright restrictions in all
 * jurisdictions.
 *   Persons may have other rights in or related to the work, such as
 * patent or trademark rights, and others may have rights in how the
 * work is used, such as publicity or privacy rights.
 *   In some jurisdictions moral rights of the author may persist beyond
 * the term of copyright. These rights may include the right to be
 * identified as the author and the right to object to derogatory treatments.
 *   Unless expressly stated otherwise, the person who identified the
 * work makes no warranties about the work, and disclaims liability for
 * all uses of the work, to the fullest extent permitted by applicable law.
 *   When using or citing the work, you should not imply endorsement by
 * the author or the person who identified the work.
 */

#ifndef __LIBRIS_H_
#define __LIBRIS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define maskA  0xFF000000
#define maskB  0x00FF0000
#define maskG  0x0000FF00
#define maskR  0x000000FF
#define BYTE_SIZE_RGBA_4BPP  4

#ifndef ABS
#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef TRIM
#define TRIM(x,a,b) (MIN(MAX((x), (a)), (b)))
#endif

enum Scaler {
    SCALER_GSAMPLE,
    SCALER_HRIS,
    SCALER_MEAN
};

enum SFactor {
    SCALER_X2 = 2,
    SCALER_X3 = 3
};

typedef struct
{
    uint8_t c[4];
} ARGBpixel;

uint8_t ByteClamp(int c);
ARGBpixel ARGBtoPixel(uint32_t targb);
uint32_t PixeltoARGB(ARGBpixel tp);

void gsample(uint32_t *src, uint32_t *dst, int width, int height, int scale_factor);
void gsample_2x(uint32_t *src,  uint32_t *dst, int width, int height);
void gsample_3x(uint32_t *src,  uint32_t *dst, int width, int height);

void scaler_hris(uint32_t *src, uint32_t *dst, int width, int height, int scale_factor);
void scaler_hris_2x(uint32_t *src,  uint32_t *dst, int width, int height);
void scaler_hris_3x(uint32_t *src,  uint32_t *dst, int width, int height);

void scaler_mean_x(uint32_t *src, uint32_t *dst, int width, int height, int scale_factor);
void scaler_mean_2x(uint32_t *src,  uint32_t *dst, int width, int height);
void scaler_mean_3x(uint32_t *src,  uint32_t *dst, int width, int height);

#ifdef __cplusplus
}
#endif

#endif //__LIBRIS_H_//
