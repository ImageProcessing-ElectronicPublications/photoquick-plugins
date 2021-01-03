/* This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed
 *
 * Copyright 2020           Arindam Chaudhuri <ksharindam@gmail.com>
 *           2007,2009,2018 Øyvind Kolås     <pippin@gimp.org>
 *           2007           Ivar Farup       <ivarf@hig.no>
 *           2007           Allesandro Rizzi <rizzi@dti.unimi.it>
 */
/*
Color to grayscale conversion, uses envelopes formed with the STRESS approach
    to perform local color-difference preserving grayscale generation.

 STRESS, Spatio Temporal Retinex Envelope with Stochastic Sampling
*/
#include <QImage>
#include <cmath>
#include <ctime>

inline float srgb_to_linear(float value)
{
    if (value > 0.04045f)
        return powf((value + 0.055f) / 1.055f, 2.4f);
    return value / 12.92f;
}

inline float linear_to_srgb(float value)
{
  if (value > 0.003131f)
    return 1.055f * powf(value, (1.0f/2.4f)) - 0.055f;
  return 12.92f * value;
}

class Image {
public:
    int _width;
    int _height;
    float *data;

    Image(int width, int height) : _width(width), _height(height) {
        data = (float*) malloc(width*height*4*sizeof(float));
    }
    // create linear image from QImage, format must be RGB32 or ARGB32
    Image(QImage &image) : Image(image.width(), image.height()) {
        // create srgb to linear conversion table
        float linear[256];
        for (int i=0; i<256; i++) {
            linear[i] = srgb_to_linear(i/255.0f);
        }
        float *pix = data;

        for (int y=0; y<_height; y++)
        {
            QRgb *row = (QRgb*) image.scanLine(y);
            for (int x=0; x<_width; x++) {
                int clr = row[x];
                pix[0] = linear[qRed(clr)];
                pix[1] = linear[qGreen(clr)];
                pix[2] = linear[qBlue(clr)];
                pix[3] = qAlpha(clr)/255.0f;
                pix += 4;
            }
        }
    }
    ~Image() {
        free(data);
    }
    int width() { return _width;}
    int height() { return _height;}
    float* scanLine(int row)   { return data + 4*(_width*row); }
    float* pixel(int x, int y) { return data + 4*(_width*y+x); }
};

#define PI           3.141593f
#define ANGLE_PRIME  95273 /* the lookuptables are sized as primes to ensure */
#define RADIUS_PRIME 29537 /* as good as possible variation when using both */

static float   lut_cos[ANGLE_PRIME];
static float   lut_sin[ANGLE_PRIME];
static float   radiuses[RADIUS_PRIME];
static double  luts_computed = 0.0;
static int     angle_no=0;
static int     radius_no=0;

/* compute lookuptables for the radial gamma */
static void compute_luts(double rgamma)
{
    if (luts_computed==rgamma)
        return;
    luts_computed = rgamma;

    float golden_angle = PI * (3-sqrt(5.0)); /* http://en.wikipedia.org/wiki/Golden_angle */
    float angle = 0.0;

    for (int i=0; i<ANGLE_PRIME; i++)
    {
        angle += golden_angle;
        lut_cos[i] = cos(angle);
        lut_sin[i] = sin(angle);
    }
    srand((unsigned)time(0));

    for (int i=0; i<RADIUS_PRIME; i++)
    {
        radiuses[i] = pow(rand()/(float)RAND_MAX, rgamma);
    }
}

static inline void
sample_min_max (Image &image,
                int         x,
                int         y,
                float      *pixel,
                int         radius,
                int         samples,//4
                float      *min,
                float      *max)
{
    float best_min[3];
    float best_max[3];
    int width = image.width();
    int height = image.height();

    for (int c=0; c<3; c++){
        best_min[c] = pixel[c];
        best_max[c] = pixel[c];
    }

    for (int i=0; i<samples; i++) {
        int u, v;
        int angle;
        float rmag;
        int max_retries = samples;

retry:/* if we've sampled outside the valid image area, we grab another
        sample instead, this should potentially work better than
        mirroring or extending with an abyss policy */

        angle = angle_no++;
        rmag = radiuses[radius_no++] * radius;

        if (angle_no>=ANGLE_PRIME)
            angle_no=0;
        if (radius_no>=RADIUS_PRIME)
            radius_no=0;

        u = x + rmag * lut_cos[angle];
        v = y + rmag * lut_sin[angle];

        if (u>=width ||
            u<0 ||
            v>=height ||
            v<0)
        {
            goto retry;
        }

      {
        pixel = image.pixel(u,v);

        if (pixel[3] > 0) /* ignore fully transparent pixels */
        {
            for (int c=0; c<3; c++)
            {
                if (pixel[c] < best_min[c])
                    best_min[c] = pixel[c];

                if (pixel[c] > best_max[c])
                    best_max[c] = pixel[c];
            }
        }
        else {
            max_retries--;
            if (max_retries > 0)
                goto retry;
        }
      }
    }
    for (int c=0; c<3; c++) {
        min[c] = best_min[c];
        max[c] = best_max[c];
    }
}


static inline void
compute_envelopes (Image &image, int x, int y,
                  int     radius,
                  int     samples,
                  int     iterations,
                  float  *min_envelope,
                  float  *max_envelope)
{
    float  range_sum[4]               = {0,0,0,0};
    float  relative_brightness_sum[4] = {0,0,0,0};

    float *pixel = image.pixel(x,y);

    for (int i=0; i<iterations; i++)
    {
        float min[3], max[3];

        // sadly, this function can not be run in parallel, because radius_no
        // and angle_no used in this function depends on previous value.
        sample_min_max (image, x, y, pixel, radius, samples, min, max);

        for (int c=0; c<3; c++)
        {
            float range, relative_brightness;

            range = max[c] - min[c];

            if (range > 0.0) {
                relative_brightness = (pixel[c] - min[c]) / range;
            }
            else {
                relative_brightness = 0.5;
            }

            relative_brightness_sum[c] += relative_brightness;
            range_sum[c] += range;
        }
    }

    for (int c=0; c<3; c++)
    {
        float relative_brightness = relative_brightness_sum[c] / iterations;
        float range               = range_sum[c] / iterations;

        max_envelope[c] = pixel[c] + (1.0 - relative_brightness) * range;

        min_envelope[c] = pixel[c] - relative_brightness * range;
    }
}

// Gamma applied to radial distribution
#define RGAMMA 2.0

/*
Radius -> Neighborhood taken into account, this is the radius in pixels taken
        into account when deciding which colors map to which gray values
Samples -> Number of samples to do per iteration looking for the range of colors
Iterations -> Number of iterations, a higher number of iterations
        provides less noisy results at a computational cost
*/
QImage
color2gray (QImage &image, int radius, int samples, int iterations, bool enhance_shadows)
{
    int w = image.width();
    int h = image.height();
    QImage dstImg(w, h, image.format());

    // create linear image buffer
    Image img(image);

    compute_luts(RGAMMA);

    for (int y=0; y < h; y++)
    {
        QRgb *dst_row = (QRgb*) dstImg.scanLine(y);
        for (int x=0; x < w; x++)
        {
            float *pixel = img.pixel(x,y);
            float  min[4], max[4];

            compute_envelopes (img, x, y,
                             radius, samples, iterations,
                             min, max);
            {
            /* this should be replaced with a better/faster projection of
             * pixel onto the vector spanned by min -> max, currently
             * computed by comparing the distance to min with the sum
             * of the distance to min/max.
             */
            float nominator = 0;
            float denominator = 0;

            if (enhance_shadows) {
                for (int c=0; c<3; c++) {
                    nominator   += (pixel[c] - min[c]) * (pixel[c] - min[c]);
                    denominator += (pixel[c] - max[c]) * (pixel[c] - max[c]);
                }
            }
            else {
                for (int c=0; c<3; c++) {
                    nominator   += pixel[c] * pixel[c];
                    denominator += (pixel[c] - max[c]) * (pixel[c] - max[c]);
                }
            }

            nominator = sqrtf (nominator);
            denominator = sqrtf (denominator);
            denominator = nominator + denominator;

            int val = 187;
            if (denominator>0.0f) {
                val = 255 * linear_to_srgb(nominator/denominator);
            }
            dst_row[x] = qRgba(val, val, val, pixel[3]*255);
            }
        }
    }
    return dstImg;
}

