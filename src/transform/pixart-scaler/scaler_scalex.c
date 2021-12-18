//
// scaler_scalex.c
//

// ========================
//
// ScaleNx pixel art scaler
//
// See: https://www.scale2x.it/
// Also: https://web.archive.org/web/20140331104356/http://gimpscripts.com/2014/01/scale2x-plus/
//
// Adapted to C (was python) from : https://opengameart.org/forumtopic/pixelart-scaler-scalenx-and-eaglenx-for-gimp
//
// ========================

#include "scaler.h"

static const char TRUE  = 1;
static const char FALSE = 0;
static const char BYTE_SIZE_RGBA_4BPP = 4; // RGBA 4BPP

// Copy a pixel from src to dst
void pixel_copy(uint8_t * dst, uint32_t dpos, uint8_t * src, uint32_t spos, int bpp)
{
    int i;
    for(i=0; i < bpp; i++)
        dst[dpos + i] = src[spos + i];
}

// Check if two pixels are equal
// TODO: RGBA Alpha handling, ignore Alpha byte?
uint8_t pixel_eql(uint8_t * src, uint32_t pos0, uint32_t pos1, int bpp)
{
    int i;

    if (pos0 == pos1)
        return TRUE;

    for(i=0; i < bpp; i++)
        if (src[pos0 + i] != src[pos1 + i])
            return FALSE;

    return TRUE;
}

uint32_t pixel_eqfuzz(uint8_t * src, uint32_t pos0, uint32_t pos1, uint32_t pos2, uint32_t pos3, int bpp)
{
    int i, d0, d1;
    uint32_t pos;

    d0 = 0;
    d1 = 0;
    for(i = 0; i < bpp; i++)
    {
        d0 += (src[pos0 + i] > src[pos1 + i]) ? (src[pos0 + i] - src[pos1 + i]) : (src[pos1 + i] - src[pos0 + i]);
        d0 += (src[pos0 + i] > src[pos2 + i]) ? (src[pos0 + i] - src[pos2 + i]) : (src[pos2 + i] - src[pos0 + i]);
        d1 += (src[pos3 + i] > src[pos1 + i]) ? (src[pos3 + i] - src[pos1 + i]) : (src[pos1 + i] - src[pos3 + i]);
        d1 += (src[pos3 + i] > src[pos2 + i]) ? (src[pos3 + i] - src[pos2 + i]) : (src[pos2 + i] - src[pos3 + i]);
    }
    d0 /= 3;
    pos = (d0 < d1) ? pos0 : pos3;
    return pos;
}

// Return adjacent pixel values for given pixel
void scale_scale2x(uint8_t * src, uint32_t * ret_pos, int x, int y, int w, int h, int bpp)
{
    int x0, y0, x2, y2;
    uint32_t pB, pD, pE, pF, pH;

    x0 = (x > 0) ? (x - 1) : 0;
    x2 = (x < w - 1) ? (x + 1) : (w - 1);
    y0 = (y > 0) ? (y - 1) : 0;
    y2 = (y < h - 1) ? (y + 1) : (h - 1);

    x0 *= bpp;
    x  *= bpp;
    x2 *= bpp;
    y0 *= bpp * w;
    y  *= bpp * w;
    y2 *= bpp * w;

    pB = x  + y0;
    pD = x0 + y;
    pE = x  + y;
    pF = x2 + y;
    pH = x  + y2;

    if ((!pixel_eql(src, pB, pH, bpp)) && (!pixel_eql(src, pD, pF, bpp)))
    {
        ret_pos[0] = (pixel_eql(src, pB, pD, bpp)) ? pD : pE;
        ret_pos[1] = (pixel_eql(src, pB, pF, bpp)) ? pF : pE;
        ret_pos[2] = (pixel_eql(src, pH, pD, bpp)) ? pD : pE;
        ret_pos[3] = (pixel_eql(src, pH, pF, bpp)) ? pF : pE;
    }
    else
    {
        ret_pos[0] = ret_pos[1] = ret_pos[2] = ret_pos[3] = pE;
    }
}

void scale_scale3x(uint8_t * src, uint32_t * ret_pos, int x, int y, int w, int h, int bpp)
{
    int x0, y0, x2, y2;
    uint32_t pA, pB, pC, pD, pE, pF, pG, pH, pI;
    uint8_t  D_B, D_H, F_B, F_H, E_A, E_G, E_C, E_I;

    x0 = (x > 0) ? (x - 1) : 0;
    x2 = (x < w - 1) ? (x + 1) : (w - 1);
    y0 = (y > 0) ? (y - 1) : 0;
    y2 = (y < h - 1) ? (y + 1) : (h - 1);

    x0 *= bpp;
    x  *= bpp;
    x2 *= bpp;
    y0 *= bpp * w;
    y  *= bpp * w;
    y2 *= bpp * w;


    pA = x0 + y0;
    pB = x  + y0;
    pC = x2 + y0;
    pD = x0 + y;
    pE = x  + y;
    pF = x2 + y;
    pG = x0 + y2;
    pH = x  + y2;
    pI = x2 + y2;

    if ((!pixel_eql(src, pB, pH, bpp)) && (!pixel_eql(src, pD, pF, bpp)))
    {
        D_B = pixel_eql(src, pD, pB, bpp);
        D_H = pixel_eql(src, pD, pH, bpp);
        F_B = pixel_eql(src, pF, pB, bpp);
        F_H = pixel_eql(src, pF, pH, bpp);

        E_A = pixel_eql(src, pE, pA, bpp);
        E_G = pixel_eql(src, pE, pG, bpp);
        E_C = pixel_eql(src, pE, pC, bpp);
        E_I = pixel_eql(src, pE, pI, bpp);

        ret_pos[0] = (D_B) ? pD : pE;
        ret_pos[1] = ((D_B && (!E_C)) || (F_B && (!E_A))) ? pB : pE;
        ret_pos[2] = (F_B) ? pF : pE;
        ret_pos[3] = ((D_B && (!E_G)) || (D_H && (!E_A))) ? pD : pE;
        ret_pos[4] = pE;
        ret_pos[5] = ((F_B && (!E_I)) || (F_H && (!E_C))) ? pF : pE;
        ret_pos[6] = (D_H) ? pD : pE;
        ret_pos[7] = ((D_H && (!E_I)) || (F_H && (!E_G))) ? pH : pE;
        ret_pos[8] = (F_H) ? pF : pE;
    }
    else
    {
        ret_pos[0] = ret_pos[1] = ret_pos[2] = pE;
        ret_pos[3] = ret_pos[4] = ret_pos[5] = pE;
        ret_pos[6] = ret_pos[7] = ret_pos[8] = pE;
    }
}

// scaler_scalex_2x
//
// Scales image in *sp up by 2x into *dp
//
// *sp : pointer to source uint32 buffer of Xres * Yres, 4BPP RGBA
// *dp : pointer to output uint32 buffer of 2 * Xres * 2 * Yres, 4BPP RGBA
// Xres, Yres: resolution of source image
//
void scaler_scalex_2x(uint32_t * sp,  uint32_t * dp, int Xres, int Yres)
{
    int       bpp;
    int       x, y;
    uint32_t  pos;
    uint32_t  return_pos[4];
    uint8_t * src, * dst;

    bpp = BYTE_SIZE_RGBA_4BPP;  // Assume 4BPP RGBA
    src = (uint8_t *) sp;
    dst = (uint8_t *) dp;


    for (y=0; y < Yres; y++)
        for (x=0; x < Xres; x++)
        {
            scale_scale2x(src, &return_pos[0], x, y, Xres, Yres, bpp);

            pos = (4 * y * Xres + 2 * x) * bpp;
            pixel_copy(dst, pos,       src, return_pos[0], bpp);
            pixel_copy(dst, pos + bpp, src, return_pos[1], bpp);

            pos += 2 * Xres * bpp;
            pixel_copy(dst, pos,       src, return_pos[2], bpp);
            pixel_copy(dst, pos + bpp, src, return_pos[3], bpp);
        }
}

// scaler_scalex_3x
//
// Scales image in *sp up by 3x into *dp
//
// *sp : pointer to source uint32 buffer of Xres * Yres, 4BPP RGBA
// *dp : pointer to output uint32 buffer of 3 * Xres * 3 * Yres, 4BPP RGBA
// Xres, Yres: resolution of source image
//
void scaler_scalex_3x(uint32_t * sp,  uint32_t * dp, int Xres, int Yres)
{
    int       bpp;
    int       x, y;
    uint32_t  pos;
    uint32_t  return_pos[9];
    uint8_t * src, * dst;

    bpp = BYTE_SIZE_RGBA_4BPP;  // Assume 4BPP RGBA
    src = (uint8_t *) sp;
    dst = (uint8_t *) dp;

    for (y=0; y < Yres; y++)
        for (x=0; x < Xres; x++)
        {
            scale_scale3x(src, &return_pos[0], x, y, Xres, Yres, bpp);


            pos = (9 * y * Xres + 3 * x) * bpp;
            pixel_copy(dst, pos,           src, return_pos[0], bpp);
            pixel_copy(dst, pos + bpp,     src, return_pos[1], bpp);
            pixel_copy(dst, pos + 2 * bpp, src, return_pos[2], bpp);

            pos += 3 * Xres * bpp;
            pixel_copy(dst, pos,           src, return_pos[3], bpp);
            pixel_copy(dst, pos + bpp,     src, return_pos[4], bpp);
            pixel_copy(dst, pos + 2 * bpp, src, return_pos[5], bpp);

            pos += 3 * Xres * bpp;
            pixel_copy(dst, pos,           src, return_pos[6], bpp);
            pixel_copy(dst, pos + bpp,     src, return_pos[7], bpp);
            pixel_copy(dst, pos + 2 * bpp, src, return_pos[8], bpp);

        }
}

// scaler_scalex_4x
//
// 4x is just the 2x scaler run twice
// Scales image in *sp up by 4x into *dp
//
// *sp : pointer to source uint32 buffer of Xres * Yres, 4BPP RGBA
// *dp : pointer to output uint32 buffer of 4 * Xres * 4 * Yres, 4BPP RGBA
// Xres, Yres: resolution of source image
//
void scaler_scalex_4x(uint32_t * sp,  uint32_t * dp, int Xres, int Yres)
{
    long       buffer_size_bytes_2x;
    uint32_t * p_tempbuf;

    // Apply the first 2x scaling
    scaler_scalex_2x(sp, dp, Xres, Yres);

    // Copy the 2x scaled image into a temp buffer
    // then scale it up 2x again
    buffer_size_bytes_2x = Xres * 2 * Yres * 2 * BYTE_SIZE_RGBA_4BPP;
    p_tempbuf = (uint32_t*) malloc(buffer_size_bytes_2x);

    memcpy(p_tempbuf, dp, buffer_size_bytes_2x);

    // Apply the second 2x scaling
    scaler_scalex_2x(p_tempbuf, dp, Xres * 2, Yres * 2);

    free(p_tempbuf);
}

void scaler_scalex(uint32_t * sp,  uint32_t * dp, int Xres, int Yres, int scalefactor)
{
    switch (scalefactor)
    {
    case 2:
        scaler_scalex_2x(sp, dp, Xres, Yres);
        break;
    case 3:
        scaler_scalex_3x(sp, dp, Xres, Yres);
        break;
    case 4:
        scaler_scalex_4x(sp, dp, Xres, Yres);
        break;
    default:
        break;
    }
}

void scale_eagle2x(uint8_t * src, uint32_t * ret_pos, int x, int y, int w, int h, int bpp)
{
    int x0, y0, x2, y2;
    uint32_t pA, pB, pC, pD, pE, pF, pG, pH, pI;

    x0 = (x > 0) ? (x - 1) : 0;
    x2 = (x < w - 1) ? (x + 1) : (w - 1);
    y0 = (y > 0) ? (y - 1) : 0;
    y2 = (y < h - 1) ? (y + 1) : (h - 1);

    x0 *= bpp;
    x  *= bpp;
    x2 *= bpp;
    y0 *= bpp * w;
    y  *= bpp * w;
    y2 *= bpp * w;


    pA = x0 + y0;
    pB = x  + y0;
    pC = x2 + y0;
    pD = x0 + y;
    pE = x  + y;
    pF = x2 + y;
    pG = x0 + y2;
    pH = x  + y2;
    pI = x2 + y2;

    ret_pos[0] = pixel_eqfuzz(src, pE, pB, pD, pA, bpp);
    ret_pos[1] = pixel_eqfuzz(src, pE, pB, pF, pC, bpp);
    ret_pos[2] = pixel_eqfuzz(src, pE, pD, pH, pG, bpp);
    ret_pos[3] = pixel_eqfuzz(src, pE, pF, pH, pI, bpp);
}

void scale_eagle3x(uint8_t * src, uint32_t * ret_pos, int x, int y, int w, int h, int bpp)
{
    int x0, y0, x2, y2;
    uint32_t pA, pB, pC, pD, pE, pF, pG, pH, pI;

    x0 = (x > 0) ? (x - 1) : 0;
    x2 = (x < w - 1) ? (x + 1) : (w - 1);
    y0 = (y > 0) ? (y - 1) : 0;
    y2 = (y < h - 1) ? (y + 1) : (h - 1);

    x0 *= bpp;
    x  *= bpp;
    x2 *= bpp;
    y0 *= bpp * w;
    y  *= bpp * w;
    y2 *= bpp * w;


    pA = x0 + y0;
    pB = x  + y0;
    pC = x2 + y0;
    pD = x0 + y;
    pE = x  + y;
    pF = x2 + y;
    pG = x0 + y2;
    pH = x  + y2;
    pI = x2 + y2;

    ret_pos[0] = pixel_eqfuzz(src, pE, pB, pD, pA, bpp);
    ret_pos[1] = pixel_eqfuzz(src, pE, pD, pF, pB, bpp);
    ret_pos[2] = pixel_eqfuzz(src, pE, pB, pF, pC, bpp);
    ret_pos[3] = pixel_eqfuzz(src, pE, pB, pH, pD, bpp);
    ret_pos[4] = pE;
    ret_pos[5] = pixel_eqfuzz(src, pE, pB, pH, pF, bpp);
    ret_pos[6] = pixel_eqfuzz(src, pE, pD, pH, pG, bpp);
    ret_pos[7] = pixel_eqfuzz(src, pE, pD, pF, pH, bpp);
    ret_pos[8] = pixel_eqfuzz(src, pE, pF, pH, pI, bpp);
}

// scaler_eagle_2x
//
// Scales image in *sp up by 2x into *dp
//
// *sp : pointer to source uint32 buffer of Xres * Yres, 4BPP RGBA
// *dp : pointer to output uint32 buffer of 2 * Xres * 2 * Yres, 4BPP RGBA
// Xres, Yres: resolution of source image
//
void scaler_eagle_2x(uint32_t * sp,  uint32_t * dp, int Xres, int Yres)
{
    int       bpp;
    int       x, y;
    uint32_t  pos;
    uint32_t  return_pos[4];
    uint8_t * src, * dst;

    bpp = BYTE_SIZE_RGBA_4BPP;  // Assume 4BPP RGBA
    src = (uint8_t *) sp;
    dst = (uint8_t *) dp;


    for (y=0; y < Yres; y++)
        for (x=0; x < Xres; x++)
        {
            scale_eagle2x(src, &return_pos[0], x, y, Xres, Yres, bpp);

            pos = (4 * y * Xres + 2 * x) * bpp;
            pixel_copy(dst, pos,       src, return_pos[0], bpp);
            pixel_copy(dst, pos + bpp, src, return_pos[1], bpp);

            pos += 2 * Xres * bpp;
            pixel_copy(dst, pos,       src, return_pos[2], bpp);
            pixel_copy(dst, pos + bpp, src, return_pos[3], bpp);
        }
}

// scaler_eagle_3x
//
// Scales image in *sp up by 3x into *dp
//
// *sp : pointer to source uint32 buffer of Xres * Yres, 4BPP RGBA
// *dp : pointer to output uint32 buffer of 3 * Xres * 3 * Yres, 4BPP RGBA
// Xres, Yres: resolution of source image
//
void scaler_eagle_3x(uint32_t * sp,  uint32_t * dp, int Xres, int Yres)
{
    int       bpp;
    int       x, y;
    uint32_t  pos;
    uint32_t  return_pos[9];
    uint8_t * src, * dst;

    bpp = BYTE_SIZE_RGBA_4BPP;  // Assume 4BPP RGBA
    src = (uint8_t *) sp;
    dst = (uint8_t *) dp;

    for (y=0; y < Yres; y++)
        for (x=0; x < Xres; x++)
        {
            scale_eagle3x(src, &return_pos[0], x, y, Xres, Yres, bpp);


            pos = (9 * y * Xres + 3 * x) * bpp;
            pixel_copy(dst, pos,           src, return_pos[0], bpp);
            pixel_copy(dst, pos + bpp,     src, return_pos[1], bpp);
            pixel_copy(dst, pos + 2 * bpp, src, return_pos[2], bpp);

            pos += 3 * Xres * bpp;
            pixel_copy(dst, pos,           src, return_pos[3], bpp);
            pixel_copy(dst, pos + bpp,     src, return_pos[4], bpp);
            pixel_copy(dst, pos + 2 * bpp, src, return_pos[5], bpp);

            pos += 3 * Xres * bpp;
            pixel_copy(dst, pos,           src, return_pos[6], bpp);
            pixel_copy(dst, pos + bpp,     src, return_pos[7], bpp);
            pixel_copy(dst, pos + 2 * bpp, src, return_pos[8], bpp);

        }
}

// scaler_eagle_4x
//
// 4x is just the 2x scaler run twice
// Scales image in *sp up by 4x into *dp
//
// *sp : pointer to source uint32 buffer of Xres * Yres, 4BPP RGBA
// *dp : pointer to output uint32 buffer of 4 * Xres * 4 * Yres, 4BPP RGBA
// Xres, Yres: resolution of source image
//
void scaler_eagle_4x(uint32_t * sp,  uint32_t * dp, int Xres, int Yres)
{
    long       buffer_size_bytes_2x;
    uint32_t * p_tempbuf;

    // Apply the first 2x scaling
    scaler_eagle_2x(sp, dp, Xres, Yres);

    // Copy the 2x scaled image into a temp buffer
    // then scale it up 2x again
    buffer_size_bytes_2x = Xres * 2 * Yres * 2 * BYTE_SIZE_RGBA_4BPP;
    p_tempbuf = (uint32_t*) malloc(buffer_size_bytes_2x);

    memcpy(p_tempbuf, dp, buffer_size_bytes_2x);

    // Apply the second 2x scaling
    scaler_eagle_2x(p_tempbuf, dp, Xres * 2, Yres * 2);

    free(p_tempbuf);
}

void scaler_eagle(uint32_t * sp,  uint32_t * dp, int Xres, int Yres, int scalefactor)
{
    switch (scalefactor)
    {
    case 2:
        scaler_eagle_2x(sp, dp, Xres, Yres);
        break;
    case 3:
        scaler_eagle_3x(sp, dp, Xres, Yres);
        break;
    case 4:
        scaler_eagle_4x(sp, dp, Xres, Yres);
        break;
    default:
        break;
    }
}
