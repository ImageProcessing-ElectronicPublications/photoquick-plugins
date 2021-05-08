/*
 * Copyright (C) 2010 Cameron Zemek ( grom@zeminvaders.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "scaler.h"

static inline uint32_t rgb_to_yuv(uint32_t c)
{
    // Mask against MASK_RGB to discard the alpha channel
    uint32_t r, g, b, y, u, v;
    c &= MASK_RGB;
    r = (c & 0xFF0000) >> 16;
    g = (c & 0x00FF00) >> 8;
    b = c & 0x0000FF;
    y = (uint32_t)(0.299*r + 0.587*g + 0.114*b);
    u = (uint32_t)(-0.169*r - 0.331*g + 0.5*b) + 128;
    v = (uint32_t)(0.5*r - 0.419*g - 0.081*b) + 128;
    return ((y << 16) + (u << 8) + v);
}

/* Test if there is difference in color */
static inline int yuv_diff(uint32_t yuv1, uint32_t yuv2)
{
    return (( abs((yuv1 & MASK_1) - (yuv2 & MASK_1)) > trY ) ||
            ( abs((yuv1 & MASK_2) - (yuv2 & MASK_2)) > trU ) ||
            ( abs((yuv1 & MASK_3) - (yuv2 & MASK_3)) > trV ) );
}

static inline int Diff(uint32_t c1, uint32_t c2)
{
    return yuv_diff(rgb_to_yuv(c1), rgb_to_yuv(c2));
}

/* Interpolate functions */
static inline uint32_t Interpolate_2(uint32_t c1, int w1, uint32_t c2, int w2, int s)
{
    if (c1 == c2)
    {
        return c1;
    }
    return
        (((((c1 & MASK_ALPHA) >> 24) * w1 + ((c2 & MASK_ALPHA) >> 24) * w2) << (24-s)) & MASK_ALPHA) +
        ((((c1 & MASK_2) * w1 + (c2 & MASK_2) * w2) >> s) & MASK_2)    +
        ((((c1 & MASK_13) * w1 + (c2 & MASK_13) * w2) >> s) & MASK_13);
}

static inline uint32_t Interpolate_3(uint32_t c1, int w1, uint32_t c2, int w2, uint32_t c3, int w3, int s)
{
    return
        (((((c1 & MASK_ALPHA) >> 24) * w1 + ((c2 & MASK_ALPHA) >> 24) * w2 + ((c3 & MASK_ALPHA) >> 24) * w3) << (24-s)) & MASK_ALPHA) +
        ((((c1 & MASK_2) * w1 + (c2 & MASK_2) * w2 + (c3 & MASK_2) * w3) >> s) & MASK_2) +
        ((((c1 & MASK_13) * w1 + (c2 & MASK_13) * w2 + (c3 & MASK_13) * w3) >> s) & MASK_13);
}

static inline void Interp1(uint32_t * pc, uint32_t c1, uint32_t c2)
{
    //*pc = (c1*3+c2) >> 2;
    *pc = Interpolate_2(c1, 3, c2, 1, 2);
}

static inline void Interp2(uint32_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*2+c2+c3) >> 2;
    *pc = Interpolate_3(c1, 2, c2, 1, c3, 1, 2);
}

static inline void Interp3(uint32_t * pc, uint32_t c1, uint32_t c2)
{
    //*pc = (c1*7+c2)/8;
    *pc = Interpolate_2(c1, 7, c2, 1, 3);
}

static inline void Interp4(uint32_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*2+(c2+c3)*7)/16;
    *pc = Interpolate_3(c1, 2, c2, 7, c3, 7, 4);
}

static inline void Interp5(uint32_t * pc, uint32_t c1, uint32_t c2)
{
    //*pc = (c1+c2) >> 1;
    *pc = Interpolate_2(c1, 1, c2, 1, 1);
}

static inline void Interp6(uint32_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*5+c2*2+c3)/8;
    *pc = Interpolate_3(c1, 5, c2, 2, c3, 1, 3);
}

static inline void Interp7(uint32_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*6+c2+c3)/8;
    *pc = Interpolate_3(c1, 6, c2, 1, c3, 1, 3);
}

static inline void Interp8(uint32_t * pc, uint32_t c1, uint32_t c2)
{
    //*pc = (c1*5+c2*3)/8;
    *pc = Interpolate_2(c1, 5, c2, 3, 3);
}

static inline void Interp9(uint32_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*2+(c2+c3)*3)/8;
    *pc = Interpolate_3(c1, 2, c2, 3, c3, 3, 3);
}

static inline void Interp10(uint32_t * pc, uint32_t c1, uint32_t c2, uint32_t c3)
{
    //*pc = (c1*14+c2+c3)/16;
    *pc = Interpolate_3(c1, 14, c2, 1, c3, 1, 4);
}

void hq2x_32_rb( uint32_t * sp, uint32_t srb, uint32_t * dp, uint32_t drb, int Xres, int Yres )
{
    int i, j, k;
    int prevline, nextline;
    uint32_t w[10];
    int dpL = (drb >> 2);
    int spL = (srb >> 2);
    uint8_t *sRowP = (uint8_t *) sp;
    uint8_t *dRowP = (uint8_t *) dp;
    uint32_t yuv1, yuv2;
    int pattern;
    int flag;

    //   +----+----+----+
    //   |    |    |    |
    //   | w1 | w2 | w3 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w4 | w5 | w6 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w7 | w8 | w9 |
    //   +----+----+----+

    for (j=0; j<Yres; j++)
    {
        if (j>0) prevline = -spL;
        else prevline = 0;
        if (j<Yres-1) nextline = spL;
        else nextline = 0;

        for (i=0; i<Xres; i++)
        {
            w[2] = *(sp + prevline);
            w[5] = *sp;
            w[8] = *(sp + nextline);

            if (i>0)
            {
                w[1] = *(sp + prevline - 1);
                w[4] = *(sp - 1);
                w[7] = *(sp + nextline - 1);
            }
            else
            {
                w[1] = w[2];
                w[4] = w[5];
                w[7] = w[8];
            }

            if (i<Xres-1)
            {
                w[3] = *(sp + prevline + 1);
                w[6] = *(sp + 1);
                w[9] = *(sp + nextline + 1);
            }
            else
            {
                w[3] = w[2];
                w[6] = w[5];
                w[9] = w[8];
            }

            pattern = 0;
            flag = 1;

            yuv1 = rgb_to_yuv(w[5]);

            for (k=1; k<=9; k++)
            {
                if (k==5) continue;

                if ( w[k] != w[5] )
                {
                    yuv2 = rgb_to_yuv(w[k]);
                    if (yuv_diff(yuv1, yuv2))
                        pattern |= flag;
                }
                flag <<= 1;
            }

            switch (pattern)
            {
            case 0:
            case 1:
            case 4:
            case 32:
            case 128:
            case 5:
            case 132:
            case 160:
            case 33:
            case 129:
            case 36:
            case 133:
            case 164:
            case 161:
            case 37:
            case 165:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 2:
            case 34:
            case 130:
            case 162:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 16:
            case 17:
            case 48:
            case 49:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 64:
            case 65:
            case 68:
            case 69:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 8:
            case 12:
            case 136:
            case 140:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 3:
            case 35:
            case 131:
            case 163:
            {
                Interp1(dp, w[5], w[4]);
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 6:
            case 38:
            case 134:
            case 166:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 20:
            case 21:
            case 52:
            case 53:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 144:
            case 145:
            case 176:
            case 177:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 192:
            case 193:
            case 196:
            case 197:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 96:
            case 97:
            case 100:
            case 101:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 40:
            case 44:
            case 168:
            case 172:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 9:
            case 13:
            case 137:
            case 141:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 18:
            case 50:
            {
                Interp2(dp, w[5], w[1], w[4]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 80:
            case 81:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 72:
            case 76:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 10:
            case 138:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 66:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 24:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 7:
            case 39:
            case 135:
            {
                Interp1(dp, w[5], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 148:
            case 149:
            case 180:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 224:
            case 228:
            case 225:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 41:
            case 169:
            case 45:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 22:
            case 54:
            {
                Interp2(dp, w[5], w[1], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 208:
            case 209:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 104:
            case 108:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 11:
            case 139:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 19:
            case 51:
            {
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp, w[5], w[4]);
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp6(dp, w[5], w[2], w[4]);
                    Interp9(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 146:
            case 178:
            {
                Interp2(dp, w[5], w[1], w[4]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                    Interp1(dp+dpL+1, w[5], w[8]);
                }
                else
                {
                    Interp9(dp+1, w[5], w[2], w[6]);
                    Interp6(dp+dpL+1, w[5], w[6], w[8]);
                }
                Interp2(dp+dpL, w[5], w[8], w[4]);
                break;
            }
            case 84:
            case 85:
            {
                Interp2(dp, w[5], w[4], w[2]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp6(dp+1, w[5], w[6], w[2]);
                    Interp9(dp+dpL+1, w[5], w[6], w[8]);
                }
                Interp2(dp+dpL, w[5], w[7], w[4]);
                break;
            }
            case 112:
            case 113:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL, w[5], w[4]);
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp6(dp+dpL, w[5], w[8], w[4]);
                    Interp9(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 200:
            case 204:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                    Interp1(dp+dpL+1, w[5], w[6]);
                }
                else
                {
                    Interp9(dp+dpL, w[5], w[8], w[4]);
                    Interp6(dp+dpL+1, w[5], w[8], w[6]);
                }
                break;
            }
            case 73:
            case 77:
            {
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp6(dp, w[5], w[4], w[2]);
                    Interp9(dp+dpL, w[5], w[8], w[4]);
                }
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 42:
            case 170:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[8]);
                }
                else
                {
                    Interp9(dp, w[5], w[4], w[2]);
                    Interp6(dp+dpL, w[5], w[4], w[8]);
                }
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 14:
            case 142:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[6]);
                }
                else
                {
                    Interp9(dp, w[5], w[4], w[2]);
                    Interp6(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 67:
            {
                Interp1(dp, w[5], w[4]);
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 70:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 28:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 152:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 194:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 98:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 56:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 25:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 26:
            case 31:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 82:
            case 214:
            {
                Interp2(dp, w[5], w[1], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 88:
            case 248:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 74:
            case 107:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp2(dp+1, w[5], w[3], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 27:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[3]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 86:
            {
                Interp2(dp, w[5], w[1], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp1(dp+dpL+1, w[5], w[9]);
                break;
            }
            case 216:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp1(dp+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 106:
            {
                Interp1(dp, w[5], w[1]);
                Interp2(dp+1, w[5], w[3], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 30:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 210:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp1(dp+1, w[5], w[3]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 120:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[9]);
                break;
            }
            case 75:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp1(dp+dpL, w[5], w[7]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 29:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 198:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 184:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 99:
            {
                Interp1(dp, w[5], w[4]);
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 57:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 71:
            {
                Interp1(dp, w[5], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 156:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 226:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 60:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 195:
            {
                Interp1(dp, w[5], w[4]);
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 102:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 153:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 58:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 83:
            {
                Interp1(dp, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[4]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 92:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp1(dp+1, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 202:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                Interp2(dp+1, w[5], w[3], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 78:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 154:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 114:
            {
                Interp2(dp, w[5], w[1], w[4]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 89:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 90:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 55:
            case 23:
            {
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp, w[5], w[4]);
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp6(dp, w[5], w[2], w[4]);
                    Interp9(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 182:
            case 150:
            {
                Interp2(dp, w[5], w[1], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    Interp1(dp+dpL+1, w[5], w[8]);
                }
                else
                {
                    Interp9(dp+1, w[5], w[2], w[6]);
                    Interp6(dp+dpL+1, w[5], w[6], w[8]);
                }
                Interp2(dp+dpL, w[5], w[8], w[4]);
                break;
            }
            case 213:
            case 212:
            {
                Interp2(dp, w[5], w[4], w[2]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+1, w[5], w[2]);
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp6(dp+1, w[5], w[6], w[2]);
                    Interp9(dp+dpL+1, w[5], w[6], w[8]);
                }
                Interp2(dp+dpL, w[5], w[7], w[4]);
                break;
            }
            case 241:
            case 240:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp6(dp+dpL, w[5], w[8], w[4]);
                    Interp9(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 236:
            case 232:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    Interp1(dp+dpL+1, w[5], w[6]);
                }
                else
                {
                    Interp9(dp+dpL, w[5], w[8], w[4]);
                    Interp6(dp+dpL+1, w[5], w[8], w[6]);
                }
                break;
            }
            case 109:
            case 105:
            {
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp, w[5], w[2]);
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp6(dp, w[5], w[4], w[2]);
                    Interp9(dp+dpL, w[5], w[8], w[4]);
                }
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 171:
            case 43:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    Interp1(dp+dpL, w[5], w[8]);
                }
                else
                {
                    Interp9(dp, w[5], w[4], w[2]);
                    Interp6(dp+dpL, w[5], w[4], w[8]);
                }
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 143:
            case 15:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    Interp1(dp+1, w[5], w[6]);
                }
                else
                {
                    Interp9(dp, w[5], w[4], w[2]);
                    Interp6(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 124:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp1(dp+1, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[9]);
                break;
            }
            case 203:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp1(dp+dpL, w[5], w[7]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 62:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 211:
            {
                Interp1(dp, w[5], w[4]);
                Interp1(dp+1, w[5], w[3]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 118:
            {
                Interp2(dp, w[5], w[1], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                Interp1(dp+dpL+1, w[5], w[9]);
                break;
            }
            case 217:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp1(dp+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 110:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 155:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[3]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 188:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 185:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 61:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 157:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 103:
            {
                Interp1(dp, w[5], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 227:
            {
                Interp1(dp, w[5], w[4]);
                Interp2(dp+1, w[5], w[3], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 230:
            {
                Interp2(dp, w[5], w[1], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 199:
            {
                Interp1(dp, w[5], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp2(dp+dpL, w[5], w[7], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 220:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp1(dp+1, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 158:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 234:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                Interp2(dp+1, w[5], w[3], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 242:
            {
                Interp2(dp, w[5], w[1], w[4]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 59:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 121:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 87:
            {
                Interp1(dp, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[4]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 79:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 122:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 94:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 218:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 91:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 229:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 167:
            {
                Interp1(dp, w[5], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 173:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 181:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 186:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 115:
            {
                Interp1(dp, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 93:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 206:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 205:
            case 201:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL, w[5], w[7]);
                }
                else
                {
                    Interp7(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 174:
            case 46:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp7(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 179:
            case 147:
            {
                Interp1(dp, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+1, w[5], w[3]);
                }
                else
                {
                    Interp7(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 117:
            case 116:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+1, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 189:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 231:
            {
                Interp1(dp, w[5], w[4]);
                Interp1(dp+1, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 126:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[9]);
                break;
            }
            case 219:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 125:
            {
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp, w[5], w[2]);
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp6(dp, w[5], w[4], w[2]);
                    Interp9(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+dpL+1, w[5], w[9]);
                break;
            }
            case 221:
            {
                Interp1(dp, w[5], w[2]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+1, w[5], w[2]);
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp6(dp+1, w[5], w[6], w[2]);
                    Interp9(dp+dpL+1, w[5], w[6], w[8]);
                }
                Interp1(dp+dpL, w[5], w[7]);
                break;
            }
            case 207:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    Interp1(dp+1, w[5], w[6]);
                }
                else
                {
                    Interp9(dp, w[5], w[4], w[2]);
                    Interp6(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[7]);
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 238:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    Interp1(dp+dpL+1, w[5], w[6]);
                }
                else
                {
                    Interp9(dp+dpL, w[5], w[8], w[4]);
                    Interp6(dp+dpL+1, w[5], w[8], w[6]);
                }
                break;
            }
            case 190:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    Interp1(dp+dpL+1, w[5], w[8]);
                }
                else
                {
                    Interp9(dp+1, w[5], w[2], w[6]);
                    Interp6(dp+dpL+1, w[5], w[6], w[8]);
                }
                Interp1(dp+dpL, w[5], w[8]);
                break;
            }
            case 187:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    Interp1(dp+dpL, w[5], w[8]);
                }
                else
                {
                    Interp9(dp, w[5], w[4], w[2]);
                    Interp6(dp+dpL, w[5], w[4], w[8]);
                }
                Interp1(dp+1, w[5], w[3]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 243:
            {
                Interp1(dp, w[5], w[4]);
                Interp1(dp+1, w[5], w[3]);
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp6(dp+dpL, w[5], w[8], w[4]);
                    Interp9(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 119:
            {
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp, w[5], w[4]);
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp6(dp, w[5], w[2], w[4]);
                    Interp9(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                Interp1(dp+dpL+1, w[5], w[9]);
                break;
            }
            case 237:
            case 233:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[2], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp10(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 175:
            case 47:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp10(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[6], w[8]);
                break;
            }
            case 183:
            case 151:
            {
                Interp1(dp, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp10(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 245:
            case 244:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp10(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 250:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 123:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[9]);
                break;
            }
            case 95:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[7]);
                Interp1(dp+dpL+1, w[5], w[9]);
                break;
            }
            case 222:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 252:
            {
                Interp2(dp, w[5], w[1], w[2]);
                Interp1(dp+1, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp10(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 249:
            {
                Interp1(dp, w[5], w[2]);
                Interp2(dp+1, w[5], w[3], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp10(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 235:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp2(dp+1, w[5], w[3], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp10(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 111:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp10(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp2(dp+dpL+1, w[5], w[9], w[6]);
                break;
            }
            case 63:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp10(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[8]);
                Interp2(dp+dpL+1, w[5], w[9], w[8]);
                break;
            }
            case 159:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp10(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 215:
            {
                Interp1(dp, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp10(dp+1, w[5], w[2], w[6]);
                }
                Interp2(dp+dpL, w[5], w[7], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 246:
            {
                Interp2(dp, w[5], w[1], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp10(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 254:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp10(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 253:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp10(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp10(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 251:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp10(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 239:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp10(dp, w[5], w[4], w[2]);
                }
                Interp1(dp+1, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp10(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[6]);
                break;
            }
            case 127:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp10(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp2(dp+1, w[5], w[2], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+dpL+1, w[5], w[9]);
                break;
            }
            case 191:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp10(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp10(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[8]);
                Interp1(dp+dpL+1, w[5], w[8]);
                break;
            }
            case 223:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp10(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 247:
            {
                Interp1(dp, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp10(dp+1, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp10(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            case 255:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp10(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp10(dp+1, w[5], w[2], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp10(dp+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp10(dp+dpL+1, w[5], w[6], w[8]);
                }
                break;
            }
            }
            sp++;
            dp += 2;
        }

        sRowP += srb;
        sp = (uint32_t *) sRowP;

        dRowP += drb * 2;
        dp = (uint32_t *) dRowP;
    }
}

void hq3x_32_rb( uint32_t * sp, uint32_t srb, uint32_t * dp, uint32_t drb, int Xres, int Yres )
{
    int i, j, k;
    int prevline, nextline;
    uint32_t w[10];
    int dpL = (drb >> 2);
    int spL = (srb >> 2);
    uint8_t *sRowP = (uint8_t *) sp;
    uint8_t *dRowP = (uint8_t *) dp;
    uint32_t yuv1, yuv2;
    int pattern;
    int flag;

    //   +----+----+----+
    //   |    |    |    |
    //   | w1 | w2 | w3 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w4 | w5 | w6 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w7 | w8 | w9 |
    //   +----+----+----+

    for (j=0; j<Yres; j++)
    {
        if (j>0) prevline = -spL;
        else prevline = 0;
        if (j<Yres-1) nextline = spL;
        else nextline = 0;

        for (i=0; i<Xres; i++)
        {
            w[2] = *(sp + prevline);
            w[5] = *sp;
            w[8] = *(sp + nextline);

            if (i>0)
            {
                w[1] = *(sp + prevline - 1);
                w[4] = *(sp - 1);
                w[7] = *(sp + nextline - 1);
            }
            else
            {
                w[1] = w[2];
                w[4] = w[5];
                w[7] = w[8];
            }

            if (i<Xres-1)
            {
                w[3] = *(sp + prevline + 1);
                w[6] = *(sp + 1);
                w[9] = *(sp + nextline + 1);
            }
            else
            {
                w[3] = w[2];
                w[6] = w[5];
                w[9] = w[8];
            }

            pattern = 0;
            flag = 1;


            yuv1 = rgb_to_yuv(w[5]);

            for (k=1; k<=9; k++)
            {
                if (k==5) continue;

                if ( w[k] != w[5] )
                {
                    yuv2 = rgb_to_yuv(w[k]);
                    if (yuv_diff(yuv1, yuv2))
                        pattern |= flag;
                }
                flag <<= 1;
            }

            switch (pattern)
            {
            case 0:
            case 1:
            case 4:
            case 32:
            case 128:
            case 5:
            case 132:
            case 160:
            case 33:
            case 129:
            case 36:
            case 133:
            case 164:
            case 161:
            case 37:
            case 165:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 2:
            case 34:
            case 130:
            case 162:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 16:
            case 17:
            case 48:
            case 49:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 64:
            case 65:
            case 68:
            case 69:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 8:
            case 12:
            case 136:
            case 140:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 3:
            case 35:
            case 131:
            case 163:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 6:
            case 38:
            case 134:
            case 166:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 20:
            case 21:
            case 52:
            case 53:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 144:
            case 145:
            case 176:
            case 177:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 192:
            case 193:
            case 196:
            case 197:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 96:
            case 97:
            case 100:
            case 101:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 40:
            case 44:
            case 168:
            case 172:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 9:
            case 13:
            case 137:
            case 141:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 18:
            case 50:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    Interp1(dp+2, w[5], w[3]);
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 80:
            case 81:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 72:
            case 76:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 10:
            case 138:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 66:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 24:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 7:
            case 39:
            case 135:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 148:
            case 149:
            case 180:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 224:
            case 228:
            case 225:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 41:
            case 169:
            case 45:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 22:
            case 54:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 208:
            case 209:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 104:
            case 108:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 11:
            case 139:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 19:
            case 51:
            {
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp, w[5], w[4]);
                    *(dp+1) = w[5];
                    Interp1(dp+2, w[5], w[3]);
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                    Interp1(dp+1, w[2], w[5]);
                    Interp5(dp+2, w[2], w[6]);
                    Interp1(dp+dpL+2, w[5], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 146:
            case 178:
            {
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    Interp1(dp+2, w[5], w[3]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+2, w[5], w[8]);
                }
                else
                {
                    Interp1(dp+1, w[5], w[2]);
                    Interp5(dp+2, w[2], w[6]);
                    Interp1(dp+dpL+2, w[6], w[5]);
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                Interp1(dp, w[5], w[1]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                break;
            }
            case 84:
            case 85:
            {
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+2, w[5], w[2]);
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                    Interp1(dp+dpL+2, w[6], w[5]);
                    Interp1(dp+dpL+dpL+1, w[5], w[8]);
                    Interp5(dp+dpL+dpL+2, w[6], w[8]);
                }
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                break;
            }
            case 112:
            case 113:
            {
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp1(dp+dpL+2, w[5], w[6]);
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+1, w[8], w[5]);
                    Interp5(dp+dpL+dpL+2, w[6], w[8]);
                }
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                break;
            }
            case 200:
            case 204:
            {
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp1(dp+dpL+dpL+2, w[5], w[6]);
                }
                else
                {
                    Interp1(dp+dpL, w[5], w[4]);
                    Interp5(dp+dpL+dpL, w[8], w[4]);
                    Interp1(dp+dpL+dpL+1, w[8], w[5]);
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                break;
            }
            case 73:
            case 77:
            {
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp, w[5], w[2]);
                    *(dp+dpL) = w[5];
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                    Interp1(dp+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL, w[8], w[4]);
                    Interp1(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 42:
            case 170:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                    Interp1(dp+dpL+dpL, w[5], w[8]);
                }
                else
                {
                    Interp5(dp, w[4], w[2]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[4], w[5]);
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 14:
            case 142:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                    *(dp+1) = w[5];
                    Interp1(dp+2, w[5], w[6]);
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[4], w[2]);
                    Interp1(dp+1, w[2], w[5]);
                    Interp2(dp+2, w[5], w[2], w[6]);
                    Interp1(dp+dpL, w[5], w[4]);
                }
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 67:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 70:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 28:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 152:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 194:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 98:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 56:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 25:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 26:
            case 31:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 82:
            case 214:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 88:
            case 248:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 74:
            case 107:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 27:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 86:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 216:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 106:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 30:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 210:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 120:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 75:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 29:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 198:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 184:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 99:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 57:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 71:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 156:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 226:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 60:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 195:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 102:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 153:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 58:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 83:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 92:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 202:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 78:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 154:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 114:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 89:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 90:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 55:
            case 23:
            {
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp, w[5], w[4]);
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                    Interp1(dp+1, w[2], w[5]);
                    Interp5(dp+2, w[2], w[6]);
                    Interp1(dp+dpL+2, w[5], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 182:
            case 150:
            {
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+2, w[5], w[8]);
                }
                else
                {
                    Interp1(dp+1, w[5], w[2]);
                    Interp5(dp+2, w[2], w[6]);
                    Interp1(dp+dpL+2, w[6], w[5]);
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                Interp1(dp, w[5], w[1]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                break;
            }
            case 213:
            case 212:
            {
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+2, w[5], w[2]);
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                    Interp1(dp+dpL+2, w[6], w[5]);
                    Interp1(dp+dpL+dpL+1, w[5], w[8]);
                    Interp5(dp+dpL+dpL+2, w[6], w[8]);
                }
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                break;
            }
            case 241:
            case 240:
            {
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp1(dp+dpL+2, w[5], w[6]);
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+1, w[8], w[5]);
                    Interp5(dp+dpL+dpL+2, w[6], w[8]);
                }
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                break;
            }
            case 236:
            case 232:
            {
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    Interp1(dp+dpL+dpL+2, w[5], w[6]);
                }
                else
                {
                    Interp1(dp+dpL, w[5], w[4]);
                    Interp5(dp+dpL+dpL, w[8], w[4]);
                    Interp1(dp+dpL+dpL+1, w[8], w[5]);
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                break;
            }
            case 109:
            case 105:
            {
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp, w[5], w[2]);
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                    Interp1(dp+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL, w[8], w[4]);
                    Interp1(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 171:
            case 43:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                    Interp1(dp+dpL+dpL, w[5], w[8]);
                }
                else
                {
                    Interp5(dp, w[4], w[2]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[4], w[5]);
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 143:
            case 15:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    Interp1(dp+2, w[5], w[6]);
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[4], w[2]);
                    Interp1(dp+1, w[2], w[5]);
                    Interp2(dp+2, w[5], w[2], w[6]);
                    Interp1(dp+dpL, w[5], w[4]);
                }
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 124:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 203:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 62:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 211:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 118:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 217:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 110:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 155:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 188:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 185:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 61:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 157:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 103:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 227:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 230:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 199:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 220:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 158:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 234:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 242:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 59:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 121:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 87:
            {
                Interp1(dp, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 79:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[6]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 122:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 94:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 218:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 91:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 229:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 167:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 173:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 181:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 186:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 115:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 93:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 206:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 205:
            case 201:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 174:
            case 46:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp1(dp, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 179:
            case 147:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 117:
            case 116:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+dpL+dpL+2, w[5], w[9]);
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 189:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 231:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 126:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 219:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 125:
            {
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp, w[5], w[2]);
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                    Interp1(dp+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL, w[8], w[4]);
                    Interp1(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 221:
            {
                if (Diff(w[6], w[8]))
                {
                    Interp1(dp+2, w[5], w[2]);
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                    Interp1(dp+dpL+2, w[6], w[5]);
                    Interp1(dp+dpL+dpL+1, w[5], w[8]);
                    Interp5(dp+dpL+dpL+2, w[6], w[8]);
                }
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                break;
            }
            case 207:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    Interp1(dp+2, w[5], w[6]);
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[4], w[2]);
                    Interp1(dp+1, w[2], w[5]);
                    Interp2(dp+2, w[5], w[2], w[6]);
                    Interp1(dp+dpL, w[5], w[4]);
                }
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 238:
            {
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    Interp1(dp+dpL+dpL+2, w[5], w[6]);
                }
                else
                {
                    Interp1(dp+dpL, w[5], w[4]);
                    Interp5(dp+dpL+dpL, w[8], w[4]);
                    Interp1(dp+dpL+dpL+1, w[8], w[5]);
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                break;
            }
            case 190:
            {
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+2, w[5], w[8]);
                }
                else
                {
                    Interp1(dp+1, w[5], w[2]);
                    Interp5(dp+2, w[2], w[6]);
                    Interp1(dp+dpL+2, w[6], w[5]);
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                Interp1(dp, w[5], w[1]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                break;
            }
            case 187:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                    Interp1(dp+dpL+dpL, w[5], w[8]);
                }
                else
                {
                    Interp5(dp, w[4], w[2]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[4], w[5]);
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 243:
            {
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp1(dp+dpL+2, w[5], w[6]);
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+1, w[8], w[5]);
                    Interp5(dp+dpL+dpL+2, w[6], w[8]);
                }
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                break;
            }
            case 119:
            {
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp, w[5], w[4]);
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                    Interp1(dp+1, w[2], w[5]);
                    Interp5(dp+2, w[2], w[6]);
                    Interp1(dp+dpL+2, w[5], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 237:
            case 233:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp2(dp+2, w[5], w[2], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 175:
            case 47:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                break;
            }
            case 183:
            case 151:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 245:
            case 244:
            {
                Interp2(dp, w[5], w[4], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 250:
            {
                Interp1(dp, w[5], w[1]);
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 123:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 95:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 222:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 252:
            {
                Interp1(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 249:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 235:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 111:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 63:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 159:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 215:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 246:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 254:
            {
                Interp1(dp, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp4(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 253:
            {
                Interp1(dp, w[5], w[2]);
                Interp1(dp+1, w[5], w[2]);
                Interp1(dp+2, w[5], w[2]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 251:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                }
                Interp1(dp+2, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL) = w[5];
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp3(dp+dpL, w[5], w[4]);
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+2, w[5], w[6]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 239:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                Interp1(dp+2, w[5], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+2, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp1(dp+dpL+dpL+2, w[5], w[6]);
                break;
            }
            case 127:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                    Interp3(dp+1, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp4(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp4(dp+dpL+dpL, w[5], w[8], w[4]);
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                }
                Interp1(dp+dpL+dpL+2, w[5], w[9]);
                break;
            }
            case 191:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[8]);
                Interp1(dp+dpL+dpL+1, w[5], w[8]);
                Interp1(dp+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 223:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp4(dp, w[5], w[4], w[2]);
                    Interp3(dp+dpL, w[5], w[4]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+1) = w[5];
                    *(dp+2) = w[5];
                    *(dp+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+1, w[5], w[2]);
                    Interp2(dp+2, w[5], w[2], w[6]);
                    Interp3(dp+dpL+2, w[5], w[6]);
                }
                *(dp+dpL+1) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp3(dp+dpL+dpL+1, w[5], w[8]);
                    Interp4(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 247:
            {
                Interp1(dp, w[5], w[4]);
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                Interp1(dp+dpL, w[5], w[4]);
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[4]);
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            case 255:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[4], w[2]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                }
                else
                {
                    Interp2(dp+2, w[5], w[2], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+2) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+2, w[5], w[6], w[8]);
                }
                break;
            }
            }
            sp++;
            dp += 3;
        }

        sRowP += srb;
        sp = (uint32_t *) sRowP;

        dRowP += drb * 3;
        dp = (uint32_t *) dRowP;
    }
}

void hq4x_32_rb( uint32_t * sp, uint32_t srb, uint32_t * dp, uint32_t drb, int Xres, int Yres )
{
    int i, j, k;
    int prevline, nextline;
    uint32_t w[10];
    int dpL = (drb >> 2);
    int spL = (srb >> 2);
    uint8_t *sRowP = (uint8_t *) sp;
    uint8_t *dRowP = (uint8_t *) dp;
    uint32_t yuv1, yuv2;
    int pattern;
    int flag;

    //   +----+----+----+
    //   |    |    |    |
    //   | w1 | w2 | w3 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w4 | w5 | w6 |
    //   +----+----+----+
    //   |    |    |    |
    //   | w7 | w8 | w9 |
    //   +----+----+----+

    for (j=0; j<Yres; j++)
    {
        if (j>0) prevline = -spL;
        else prevline = 0;
        if (j<Yres-1) nextline = spL;
        else nextline = 0;

        for (i=0; i<Xres; i++)
        {
            w[2] = *(sp + prevline);
            w[5] = *sp;
            w[8] = *(sp + nextline);

            if (i>0)
            {
                w[1] = *(sp + prevline - 1);
                w[4] = *(sp - 1);
                w[7] = *(sp + nextline - 1);
            }
            else
            {
                w[1] = w[2];
                w[4] = w[5];
                w[7] = w[8];
            }

            if (i<Xres-1)
            {
                w[3] = *(sp + prevline + 1);
                w[6] = *(sp + 1);
                w[9] = *(sp + nextline + 1);
            }
            else
            {
                w[3] = w[2];
                w[6] = w[5];
                w[9] = w[8];
            }

            pattern = 0;
            flag = 1;


            yuv1 = rgb_to_yuv(w[5]);

            for (k=1; k<=9; k++)
            {
                if (k==5) continue;

                if ( w[k] != w[5] )
                {
                    yuv2 = rgb_to_yuv(w[k]);
                    if (yuv_diff(yuv1, yuv2))
                        pattern |= flag;
                }
                flag <<= 1;
            }

            switch (pattern)
            {
            case 0:
            case 1:
            case 4:
            case 32:
            case 128:
            case 5:
            case 132:
            case 160:
            case 33:
            case 129:
            case 36:
            case 133:
            case 164:
            case 161:
            case 37:
            case 165:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 2:
            case 34:
            case 130:
            case 162:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 16:
            case 17:
            case 48:
            case 49:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 64:
            case 65:
            case 68:
            case 69:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 8:
            case 12:
            case 136:
            case 140:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 3:
            case 35:
            case 131:
            case 163:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 6:
            case 38:
            case 134:
            case 166:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 20:
            case 21:
            case 52:
            case 53:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 144:
            case 145:
            case 176:
            case 177:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 192:
            case 193:
            case 196:
            case 197:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 96:
            case 97:
            case 100:
            case 101:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 40:
            case 44:
            case 168:
            case 172:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 9:
            case 13:
            case 137:
            case 141:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 18:
            case 50:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 80:
            case 81:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 72:
            case 76:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 10:
            case 138:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                    *(dp+dpL+1) = w[5];
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 66:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 24:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 7:
            case 39:
            case 135:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 148:
            case 149:
            case 180:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 224:
            case 228:
            case 225:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 41:
            case 169:
            case 45:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 22:
            case 54:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                *(dp+dpL+2) = w[5];
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 208:
            case 209:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 104:
            case 108:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 11:
            case 139:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 19:
            case 51:
            {
                if (Diff(w[2], w[6]))
                {
                    Interp8(dp, w[5], w[4]);
                    Interp3(dp+1, w[5], w[4]);
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp, w[5], w[2]);
                    Interp1(dp+1, w[2], w[5]);
                    Interp8(dp+2, w[2], w[6]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp7(dp+dpL+2, w[5], w[6], w[2]);
                    Interp2(dp+dpL+3, w[6], w[5], w[2]);
                }
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 146:
            case 178:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                    Interp3(dp+dpL+dpL+3, w[5], w[8]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                }
                else
                {
                    Interp2(dp+2, w[2], w[5], w[6]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp7(dp+dpL+2, w[5], w[6], w[2]);
                    Interp8(dp+dpL+3, w[6], w[2]);
                    Interp1(dp+dpL+dpL+3, w[6], w[5]);
                    Interp1(dp+dpL+dpL+dpL+3, w[5], w[6]);
                }
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 84:
            case 85:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp8(dp+2, w[5], w[2]);
                if (Diff(w[6], w[8]))
                {
                    Interp8(dp+3, w[5], w[2]);
                    Interp3(dp+dpL+3, w[5], w[2]);
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    Interp1(dp+3, w[5], w[6]);
                    Interp1(dp+dpL+3, w[6], w[5]);
                    Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                    Interp8(dp+dpL+dpL+3, w[6], w[8]);
                    Interp2(dp+dpL+dpL+dpL+2, w[8], w[5], w[6]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 112:
            case 113:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                    Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                    Interp2(dp+dpL+dpL+3, w[6], w[5], w[8]);
                    Interp1(dp+dpL+dpL+dpL, w[5], w[8]);
                    Interp1(dp+dpL+dpL+dpL+1, w[8], w[5]);
                    Interp8(dp+dpL+dpL+dpL+2, w[8], w[6]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                break;
            }
            case 200:
            case 204:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                    Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[4], w[5], w[8]);
                    Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp8(dp+dpL+dpL+dpL+1, w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp1(dp+dpL+dpL+dpL+3, w[5], w[8]);
                }
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 73:
            case 77:
            {
                if (Diff(w[8], w[4]))
                {
                    Interp8(dp, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[2]);
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp, w[5], w[4]);
                    Interp1(dp+dpL, w[4], w[5]);
                    Interp8(dp+dpL+dpL, w[4], w[8]);
                    Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp2(dp+dpL+dpL+dpL+1, w[8], w[5], w[4]);
                }
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 42:
            case 170:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                    Interp3(dp+dpL+dpL, w[5], w[8]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp2(dp+1, w[2], w[5], w[4]);
                    Interp8(dp+dpL, w[4], w[2]);
                    Interp7(dp+dpL+1, w[5], w[4], w[2]);
                    Interp1(dp+dpL+dpL, w[4], w[5]);
                    Interp1(dp+dpL+dpL+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 14:
            case 142:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp3(dp+2, w[5], w[6]);
                    Interp8(dp+3, w[5], w[6]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp8(dp+1, w[2], w[4]);
                    Interp1(dp+2, w[2], w[5]);
                    Interp1(dp+3, w[5], w[2]);
                    Interp2(dp+dpL, w[4], w[5], w[2]);
                    Interp7(dp+dpL+1, w[5], w[4], w[2]);
                }
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 67:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 70:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 28:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 152:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 194:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 98:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 56:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 25:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 26:
            case 31:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 82:
            case 214:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                *(dp+dpL+2) = w[5];
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 88:
            case 248:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                break;
            }
            case 74:
            case 107:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 27:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 86:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                *(dp+dpL+2) = w[5];
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 216:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 106:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 30:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 210:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 120:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 75:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 29:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 198:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 184:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 99:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 57:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 71:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 156:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 226:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 60:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 195:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 102:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 153:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 58:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 83:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 92:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 202:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 78:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 154:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 114:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                break;
            }
            case 89:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 90:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 55:
            case 23:
            {
                if (Diff(w[2], w[6]))
                {
                    Interp8(dp, w[5], w[4]);
                    Interp3(dp+1, w[5], w[4]);
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp1(dp, w[5], w[2]);
                    Interp1(dp+1, w[2], w[5]);
                    Interp8(dp+2, w[2], w[6]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp7(dp+dpL+2, w[5], w[6], w[2]);
                    Interp2(dp+dpL+3, w[6], w[5], w[2]);
                }
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 182:
            case 150:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+3) = w[5];
                    Interp3(dp+dpL+dpL+3, w[5], w[8]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                }
                else
                {
                    Interp2(dp+2, w[2], w[5], w[6]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp7(dp+dpL+2, w[5], w[6], w[2]);
                    Interp8(dp+dpL+3, w[6], w[2]);
                    Interp1(dp+dpL+dpL+3, w[6], w[5]);
                    Interp1(dp+dpL+dpL+dpL+3, w[5], w[6]);
                }
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 213:
            case 212:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp8(dp+2, w[5], w[2]);
                if (Diff(w[6], w[8]))
                {
                    Interp8(dp+3, w[5], w[2]);
                    Interp3(dp+dpL+3, w[5], w[2]);
                    *(dp+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp1(dp+3, w[5], w[6]);
                    Interp1(dp+dpL+3, w[6], w[5]);
                    Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                    Interp8(dp+dpL+dpL+3, w[6], w[8]);
                    Interp2(dp+dpL+dpL+dpL+2, w[8], w[5], w[6]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 241:
            case 240:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+3) = w[5];
                    Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                    Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                    Interp2(dp+dpL+dpL+3, w[6], w[5], w[8]);
                    Interp1(dp+dpL+dpL+dpL, w[5], w[8]);
                    Interp1(dp+dpL+dpL+dpL+1, w[8], w[5]);
                    Interp8(dp+dpL+dpL+dpL+2, w[8], w[6]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                break;
            }
            case 236:
            case 232:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                    Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[4], w[5], w[8]);
                    Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp8(dp+dpL+dpL+dpL+1, w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp1(dp+dpL+dpL+dpL+3, w[5], w[8]);
                }
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 109:
            case 105:
            {
                if (Diff(w[8], w[4]))
                {
                    Interp8(dp, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[2]);
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp1(dp, w[5], w[4]);
                    Interp1(dp+dpL, w[4], w[5]);
                    Interp8(dp+dpL+dpL, w[4], w[8]);
                    Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp2(dp+dpL+dpL+dpL+1, w[8], w[5], w[4]);
                }
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 171:
            case 43:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                    *(dp+dpL+1) = w[5];
                    Interp3(dp+dpL+dpL, w[5], w[8]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp2(dp+1, w[2], w[5], w[4]);
                    Interp8(dp+dpL, w[4], w[2]);
                    Interp7(dp+dpL+1, w[5], w[4], w[2]);
                    Interp1(dp+dpL+dpL, w[4], w[5]);
                    Interp1(dp+dpL+dpL+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 143:
            case 15:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    Interp3(dp+2, w[5], w[6]);
                    Interp8(dp+3, w[5], w[6]);
                    *(dp+dpL) = w[5];
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp8(dp+1, w[2], w[4]);
                    Interp1(dp+2, w[2], w[5]);
                    Interp1(dp+3, w[5], w[2]);
                    Interp2(dp+dpL, w[4], w[5], w[2]);
                    Interp7(dp+dpL+1, w[5], w[4], w[2]);
                }
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 124:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 203:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 62:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                *(dp+dpL+2) = w[5];
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 211:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 118:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                *(dp+dpL+2) = w[5];
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 217:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 110:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 155:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 188:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 185:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 61:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 157:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 103:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 227:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 230:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 199:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 220:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                break;
            }
            case 158:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 234:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 242:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                break;
            }
            case 59:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 121:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 87:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                *(dp+dpL+2) = w[5];
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 79:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 122:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 94:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 218:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                break;
            }
            case 91:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                *(dp+dpL+1) = w[5];
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 229:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 167:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 173:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 181:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 186:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 115:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                break;
            }
            case 93:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 206:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 205:
            case 201:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                if (Diff(w[8], w[4]))
                {
                    Interp1(dp+dpL+dpL, w[5], w[7]);
                    Interp3(dp+dpL+dpL+1, w[5], w[7]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                }
                else
                {
                    Interp1(dp+dpL+dpL, w[5], w[4]);
                    *(dp+dpL+dpL+1) = w[5];
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+1, w[5], w[8]);
                }
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 174:
            case 46:
            {
                if (Diff(w[4], w[2]))
                {
                    Interp8(dp, w[5], w[1]);
                    Interp1(dp+1, w[5], w[1]);
                    Interp1(dp+dpL, w[5], w[1]);
                    Interp3(dp+dpL+1, w[5], w[1]);
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                    Interp1(dp+1, w[5], w[2]);
                    Interp1(dp+dpL, w[5], w[4]);
                    *(dp+dpL+1) = w[5];
                }
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 179:
            case 147:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                if (Diff(w[2], w[6]))
                {
                    Interp1(dp+2, w[5], w[3]);
                    Interp8(dp+3, w[5], w[3]);
                    Interp3(dp+dpL+2, w[5], w[3]);
                    Interp1(dp+dpL+3, w[5], w[3]);
                }
                else
                {
                    Interp1(dp+2, w[5], w[2]);
                    Interp2(dp+3, w[5], w[2], w[6]);
                    *(dp+dpL+2) = w[5];
                    Interp1(dp+dpL+3, w[5], w[6]);
                }
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 117:
            case 116:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    Interp3(dp+dpL+dpL+2, w[5], w[9]);
                    Interp1(dp+dpL+dpL+3, w[5], w[9]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                }
                else
                {
                    *(dp+dpL+dpL+2) = w[5];
                    Interp1(dp+dpL+dpL+3, w[5], w[6]);
                    Interp1(dp+dpL+dpL+dpL+2, w[5], w[8]);
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                break;
            }
            case 189:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 231:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 126:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 219:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 125:
            {
                if (Diff(w[8], w[4]))
                {
                    Interp8(dp, w[5], w[2]);
                    Interp3(dp+dpL, w[5], w[2]);
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp1(dp, w[5], w[4]);
                    Interp1(dp+dpL, w[4], w[5]);
                    Interp8(dp+dpL+dpL, w[4], w[8]);
                    Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp2(dp+dpL+dpL+dpL+1, w[8], w[5], w[4]);
                }
                Interp8(dp+1, w[5], w[2]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 221:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp8(dp+2, w[5], w[2]);
                if (Diff(w[6], w[8]))
                {
                    Interp8(dp+3, w[5], w[2]);
                    Interp3(dp+dpL+3, w[5], w[2]);
                    *(dp+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp1(dp+3, w[5], w[6]);
                    Interp1(dp+dpL+3, w[6], w[5]);
                    Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                    Interp8(dp+dpL+dpL+3, w[6], w[8]);
                    Interp2(dp+dpL+dpL+dpL+2, w[8], w[5], w[6]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 207:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    Interp3(dp+2, w[5], w[6]);
                    Interp8(dp+3, w[5], w[6]);
                    *(dp+dpL) = w[5];
                    *(dp+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp8(dp+1, w[2], w[4]);
                    Interp1(dp+2, w[2], w[5]);
                    Interp1(dp+3, w[5], w[2]);
                    Interp2(dp+dpL, w[4], w[5], w[2]);
                    Interp7(dp+dpL+1, w[5], w[4], w[2]);
                }
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 238:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+1) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                    Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                }
                else
                {
                    Interp2(dp+dpL+dpL, w[4], w[5], w[8]);
                    Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp8(dp+dpL+dpL+dpL+1, w[8], w[4]);
                    Interp1(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp1(dp+dpL+dpL+dpL+3, w[5], w[8]);
                }
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 190:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+3) = w[5];
                    Interp3(dp+dpL+dpL+3, w[5], w[8]);
                    Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                }
                else
                {
                    Interp2(dp+2, w[2], w[5], w[6]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp7(dp+dpL+2, w[5], w[6], w[2]);
                    Interp8(dp+dpL+3, w[6], w[2]);
                    Interp1(dp+dpL+dpL+3, w[6], w[5]);
                    Interp1(dp+dpL+dpL+dpL+3, w[5], w[6]);
                }
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                break;
            }
            case 187:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                    *(dp+dpL+1) = w[5];
                    Interp3(dp+dpL+dpL, w[5], w[8]);
                    Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp2(dp+1, w[2], w[5], w[4]);
                    Interp8(dp+dpL, w[4], w[2]);
                    Interp7(dp+dpL+1, w[5], w[4], w[2]);
                    Interp1(dp+dpL+dpL, w[4], w[5]);
                    Interp1(dp+dpL+dpL+dpL, w[5], w[4]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 243:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+3) = w[5];
                    Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                    Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                    Interp2(dp+dpL+dpL+3, w[6], w[5], w[8]);
                    Interp1(dp+dpL+dpL+dpL, w[5], w[8]);
                    Interp1(dp+dpL+dpL+dpL+1, w[8], w[5]);
                    Interp8(dp+dpL+dpL+dpL+2, w[8], w[6]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                break;
            }
            case 119:
            {
                if (Diff(w[2], w[6]))
                {
                    Interp8(dp, w[5], w[4]);
                    Interp3(dp+1, w[5], w[4]);
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+2) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp1(dp, w[5], w[2]);
                    Interp1(dp+1, w[2], w[5]);
                    Interp8(dp+2, w[2], w[6]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp7(dp+dpL+2, w[5], w[6], w[2]);
                    Interp2(dp+dpL+3, w[6], w[5], w[2]);
                }
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 237:
            case 233:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[6]);
                Interp2(dp+3, w[5], w[2], w[6]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp7(dp+dpL+2, w[5], w[6], w[2]);
                Interp6(dp+dpL+3, w[5], w[6], w[2]);
                *(dp+dpL+dpL) = w[5];
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 175:
            case 47:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                }
                *(dp+1) = w[5];
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp7(dp+dpL+dpL+2, w[5], w[6], w[8]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[6]);
                Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                break;
            }
            case 183:
            case 151:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                *(dp+2) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+3) = w[5];
                }
                else
                {
                    Interp2(dp+3, w[5], w[2], w[6]);
                }
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                *(dp+dpL+2) = w[5];
                *(dp+dpL+3) = w[5];
                Interp6(dp+dpL+dpL, w[5], w[4], w[8]);
                Interp7(dp+dpL+dpL+1, w[5], w[4], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[4]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 245:
            case 244:
            {
                Interp2(dp, w[5], w[2], w[4]);
                Interp6(dp+1, w[5], w[2], w[4]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp6(dp+dpL, w[5], w[4], w[2]);
                Interp7(dp+dpL+1, w[5], w[4], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                *(dp+dpL+dpL+2) = w[5];
                *(dp+dpL+dpL+3) = w[5];
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                *(dp+dpL+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 250:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                break;
            }
            case 123:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 95:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 222:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                *(dp+dpL+2) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 252:
            {
                Interp8(dp, w[5], w[1]);
                Interp6(dp+1, w[5], w[2], w[1]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                *(dp+dpL+dpL+2) = w[5];
                *(dp+dpL+dpL+3) = w[5];
                *(dp+dpL+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 249:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp6(dp+2, w[5], w[2], w[3]);
                Interp8(dp+3, w[5], w[3]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                *(dp+dpL+dpL) = w[5];
                *(dp+dpL+dpL+1) = w[5];
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+dpL+1) = w[5];
                break;
            }
            case 235:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp6(dp+dpL+3, w[5], w[6], w[3]);
                *(dp+dpL+dpL) = w[5];
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 111:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                }
                *(dp+1) = w[5];
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp6(dp+dpL+dpL+3, w[5], w[6], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 63:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp6(dp+dpL+dpL+dpL+2, w[5], w[8], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 159:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                *(dp+2) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+3) = w[5];
                }
                else
                {
                    Interp2(dp+3, w[5], w[2], w[6]);
                }
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                *(dp+dpL+3) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp6(dp+dpL+dpL+dpL+1, w[5], w[8], w[7]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 215:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                *(dp+2) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+3) = w[5];
                }
                else
                {
                    Interp2(dp+3, w[5], w[2], w[6]);
                }
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                *(dp+dpL+2) = w[5];
                *(dp+dpL+3) = w[5];
                Interp6(dp+dpL+dpL, w[5], w[4], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 246:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp6(dp+dpL, w[5], w[4], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                *(dp+dpL+2) = w[5];
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                *(dp+dpL+dpL+2) = w[5];
                *(dp+dpL+dpL+3) = w[5];
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                *(dp+dpL+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 254:
            {
                Interp8(dp, w[5], w[1]);
                Interp1(dp+1, w[5], w[1]);
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                Interp1(dp+dpL, w[5], w[1]);
                Interp3(dp+dpL+1, w[5], w[1]);
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                *(dp+dpL+dpL+2) = w[5];
                *(dp+dpL+dpL+3) = w[5];
                *(dp+dpL+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 253:
            {
                Interp8(dp, w[5], w[2]);
                Interp8(dp+1, w[5], w[2]);
                Interp8(dp+2, w[5], w[2]);
                Interp8(dp+3, w[5], w[2]);
                Interp3(dp+dpL, w[5], w[2]);
                Interp3(dp+dpL+1, w[5], w[2]);
                Interp3(dp+dpL+2, w[5], w[2]);
                Interp3(dp+dpL+3, w[5], w[2]);
                *(dp+dpL+dpL) = w[5];
                *(dp+dpL+dpL+1) = w[5];
                *(dp+dpL+dpL+2) = w[5];
                *(dp+dpL+dpL+3) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+dpL+1) = w[5];
                *(dp+dpL+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 251:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                Interp1(dp+2, w[5], w[3]);
                Interp8(dp+3, w[5], w[3]);
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[3]);
                Interp1(dp+dpL+3, w[5], w[3]);
                *(dp+dpL+dpL) = w[5];
                *(dp+dpL+dpL+1) = w[5];
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+dpL+1) = w[5];
                break;
            }
            case 239:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                }
                *(dp+1) = w[5];
                Interp3(dp+2, w[5], w[6]);
                Interp8(dp+3, w[5], w[6]);
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                Interp3(dp+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+3, w[5], w[6]);
                *(dp+dpL+dpL) = w[5];
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+3, w[5], w[6]);
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+dpL+2, w[5], w[6]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[6]);
                break;
            }
            case 127:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                }
                *(dp+1) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+2) = w[5];
                    *(dp+3) = w[5];
                    *(dp+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+2, w[2], w[5]);
                    Interp5(dp+3, w[2], w[6]);
                    Interp5(dp+dpL+3, w[6], w[5]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL) = w[5];
                    *(dp+dpL+dpL+dpL+1) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL, w[4], w[5]);
                    Interp5(dp+dpL+dpL+dpL, w[8], w[4]);
                    Interp5(dp+dpL+dpL+dpL+1, w[8], w[5]);
                }
                *(dp+dpL+dpL+1) = w[5];
                Interp3(dp+dpL+dpL+2, w[5], w[9]);
                Interp1(dp+dpL+dpL+3, w[5], w[9]);
                Interp1(dp+dpL+dpL+dpL+2, w[5], w[9]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[9]);
                break;
            }
            case 191:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                }
                *(dp+1) = w[5];
                *(dp+2) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+3) = w[5];
                }
                else
                {
                    Interp2(dp+3, w[5], w[2], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                *(dp+dpL+3) = w[5];
                Interp3(dp+dpL+dpL, w[5], w[8]);
                Interp3(dp+dpL+dpL+1, w[5], w[8]);
                Interp3(dp+dpL+dpL+2, w[5], w[8]);
                Interp3(dp+dpL+dpL+3, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+1, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+2, w[5], w[8]);
                Interp8(dp+dpL+dpL+dpL+3, w[5], w[8]);
                break;
            }
            case 223:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                    *(dp+1) = w[5];
                    *(dp+dpL) = w[5];
                }
                else
                {
                    Interp5(dp, w[2], w[4]);
                    Interp5(dp+1, w[2], w[5]);
                    Interp5(dp+dpL, w[4], w[5]);
                }
                *(dp+2) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+3) = w[5];
                }
                else
                {
                    Interp2(dp+3, w[5], w[2], w[6]);
                }
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                *(dp+dpL+3) = w[5];
                Interp1(dp+dpL+dpL, w[5], w[7]);
                Interp3(dp+dpL+dpL+1, w[5], w[7]);
                *(dp+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+3) = w[5];
                    *(dp+dpL+dpL+dpL+2) = w[5];
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp5(dp+dpL+dpL+3, w[6], w[5]);
                    Interp5(dp+dpL+dpL+dpL+2, w[8], w[5]);
                    Interp5(dp+dpL+dpL+dpL+3, w[8], w[6]);
                }
                Interp8(dp+dpL+dpL+dpL, w[5], w[7]);
                Interp1(dp+dpL+dpL+dpL+1, w[5], w[7]);
                break;
            }
            case 247:
            {
                Interp8(dp, w[5], w[4]);
                Interp3(dp+1, w[5], w[4]);
                *(dp+2) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+3) = w[5];
                }
                else
                {
                    Interp2(dp+3, w[5], w[2], w[6]);
                }
                Interp8(dp+dpL, w[5], w[4]);
                Interp3(dp+dpL+1, w[5], w[4]);
                *(dp+dpL+2) = w[5];
                *(dp+dpL+3) = w[5];
                Interp8(dp+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+1, w[5], w[4]);
                *(dp+dpL+dpL+2) = w[5];
                *(dp+dpL+dpL+3) = w[5];
                Interp8(dp+dpL+dpL+dpL, w[5], w[4]);
                Interp3(dp+dpL+dpL+dpL+1, w[5], w[4]);
                *(dp+dpL+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            case 255:
            {
                if (Diff(w[4], w[2]))
                {
                    *dp = w[5];
                }
                else
                {
                    Interp2(dp, w[5], w[2], w[4]);
                }
                *(dp+1) = w[5];
                *(dp+2) = w[5];
                if (Diff(w[2], w[6]))
                {
                    *(dp+3) = w[5];
                }
                else
                {
                    Interp2(dp+3, w[5], w[2], w[6]);
                }
                *(dp+dpL) = w[5];
                *(dp+dpL+1) = w[5];
                *(dp+dpL+2) = w[5];
                *(dp+dpL+3) = w[5];
                *(dp+dpL+dpL) = w[5];
                *(dp+dpL+dpL+1) = w[5];
                *(dp+dpL+dpL+2) = w[5];
                *(dp+dpL+dpL+3) = w[5];
                if (Diff(w[8], w[4]))
                {
                    *(dp+dpL+dpL+dpL) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL, w[5], w[8], w[4]);
                }
                *(dp+dpL+dpL+dpL+1) = w[5];
                *(dp+dpL+dpL+dpL+2) = w[5];
                if (Diff(w[6], w[8]))
                {
                    *(dp+dpL+dpL+dpL+3) = w[5];
                }
                else
                {
                    Interp2(dp+dpL+dpL+dpL+3, w[5], w[8], w[6]);
                }
                break;
            }
            }
            sp++;
            dp += 4;
        }

        sRowP += srb;
        sp = (uint32_t *) sRowP;

        dRowP += drb * 4;
        dp = (uint32_t *) dRowP;
    }
}

void hq2x_32( uint32_t * sp, uint32_t * dp, int Xres, int Yres )
{
    uint32_t rowBytesL = Xres * 4;
    hq2x_32_rb(sp, rowBytesL, dp, rowBytesL * 2, Xres, Yres);
}

void hq3x_32( uint32_t * sp, uint32_t * dp, int Xres, int Yres )
{
    uint32_t rowBytesL = Xres * 4;
    hq3x_32_rb(sp, rowBytesL, dp, rowBytesL * 3, Xres, Yres);
}

void hq4x_32( uint32_t * sp, uint32_t * dp, int Xres, int Yres )
{
    uint32_t rowBytesL = Xres * 4;
    hq4x_32_rb(sp, rowBytesL, dp, rowBytesL * 4, Xres, Yres);
}

void hqx(uint32_t * sp,  uint32_t * dp, int Xres, int Yres, int scalefactor)
{
    switch (scalefactor)
    {
    case 2:
        hq2x_32(sp, dp, Xres, Yres);
        break;
    case 3:
        hq3x_32(sp, dp, Xres, Yres);
        break;
    case 4:
        hq4x_32(sp, dp, Xres, Yres);
        break;
    default:
        break;
    }
}
