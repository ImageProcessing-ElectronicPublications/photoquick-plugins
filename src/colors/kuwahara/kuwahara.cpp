#include "kuwahara.h"
#include <cmath>
#include <QInputDialog>

#define PLUGIN_NAME "Kuwahara Filter"
#define PLUGIN_MENU "Filters/Effects/Kuwahara Filter"
#define PLUGIN_VERSION "1.0"

Q_EXPORT_PLUGIN2(kuwahara, FilterPlugin);


// takes 4 pixels and a floating point coordinate, returns bilinear interpolated pixel
QRgb interpolateBilinear(float x, float y, QRgb p00, QRgb p01, QRgb p10, QRgb p11)
{
    float offset_x = floorf(x);
    float offset_y = floorf(y);
    float delta_x = (x-offset_x);// w
    float delta_y = (y-offset_y);// h
    float epsilon_x = 1.0-delta_x;// 1-w
    float epsilon_y = 1.0-delta_y;// 1-h
    // Y = A(1-w)(1-h) + B(w)(1-h) + C(h)(1-w) + D(w)(h)
    //   = (1-h){(1-w)A + wB} + h{(1-w)C + wD}
    int r = (epsilon_y*(epsilon_x*qRed(p00)+delta_x*qRed(p01)) +
                delta_y*(epsilon_x*qRed(p10)+delta_x*qRed(p11)));
    int g = (epsilon_y*(epsilon_x*qGreen(p00)+delta_x*qGreen(p01)) +
                delta_y*(epsilon_x*qGreen(p10)+delta_x*qGreen(p11)));
    int b = (epsilon_y*(epsilon_x*qBlue(p00)+delta_x*qBlue(p01)) +
                delta_y*(epsilon_x*qBlue(p10)+delta_x*qBlue(p11)));
    return qRgb(r,g,b);
}

// Calculate Luminance (Y) from an sRGB value
inline float getPixelLuma(QRgb clr)
{
  return (0.212656f*qRed(clr) + 0.715158f*qGreen(clr) + 0.072186f*qBlue(clr));
}

inline double getPixelLuma(double red, double green, double blue)
{
  return (0.212656*red + 0.715158*green + 0.072186*blue);
}


typedef struct {
    int x;
    int y;
    int width;
    int height;
}RectInfo;

// Expand each size of Image by certain amount of border
QImage expandBorder(QImage img, int width)
{
    int w = img.width();
    int h = img.height();
    QImage dst = QImage(w+2*width, h+2*width, img.format());
    // copy all image pixels at the center
    QRgb *row, *dstRow;
    for (int y=0; y<h; y++) {
        row = (QRgb*)img.constScanLine(y);
        dstRow = (QRgb*)dst.scanLine(y+width);
        dstRow += width;
        memcpy(dstRow, row, w*4);
    }
    // duplicate first row
    row = (QRgb*)img.constScanLine(0);
    for (int i=0; i<width; i++) {
        dstRow = (QRgb*)dst.scanLine(i);
        dstRow += width;
        memcpy(dstRow, row, w*4);
    }
    // duplicate last row
    row = (QRgb*)img.constScanLine(h-1);
    for (int i=0; i<width; i++) {
        dstRow = (QRgb*)dst.scanLine(width+h+i);
        dstRow += width;
        memcpy(dstRow, row, w*4);
    }
    // duplicate left and right sides
    QRgb left_clr, right_clr;
    for (int y=0; y<h; y++) {
        dstRow = (QRgb*)dst.scanLine(y+width);
        left_clr = ((QRgb*)img.constScanLine(y))[0];
        right_clr = ((QRgb*)img.constScanLine(y))[w-1];
        for (int x=0; x<width; x++) {
            dstRow[x] = left_clr;
            dstRow[width+w+x] = right_clr;
        }
    }
    // duplicate corner pixels
    row = (QRgb*)img.constScanLine(0);
    for (int y=0; y<width; y++) {
        dstRow = (QRgb*)dst.scanLine(y);
        for (int x=0; x<width; x++) {
            dstRow[x] = row[0];
            dstRow[width+w+x] = row[w-1];
        }
    }
    row = (QRgb*)img.constScanLine(h-1);
    for (int y=0; y<width; y++) {
        dstRow = (QRgb*)dst.scanLine(width+h+y);
        for (int x=0; x<width; x++) {
            dstRow[x] = row[0];
            dstRow[width+w+x] = row[w-1];
        }
    }
    return dst;
}

// convolve a 1D kernel first left to right and then top to bottom
void convolve1D(QImage &img, float kernel[], int width/*of kernel*/)
{
    /* Build normalized kernel */
    float normal_kernel[width]; // = {}; // Throws error in C99 compiler
    memset(normal_kernel, 0, width * sizeof(float));

    float normalize = 0.0;
    for (int i=0; i < width; i++)
        normalize += kernel[i];
    // if (abs(normalize) == 0) normalize=1.0;
    normalize = 1.0/normalize;
    for (int i=0; i < (width); i++) {
        normal_kernel[i] = normalize * kernel[i];
    }

    int radius = width/2;
    int w = img.width();
    int h = img.height();
    /* Convolve image */
    QImage src_img = expandBorder(img, radius);
    int src_w = src_img.width();

    QRgb *data_src = (QRgb*)src_img.constScanLine(0);
    QRgb *data_dst = (QRgb*)img.scanLine(0);

    #pragma omp parallel for
    for (int y=0; y < h; y++)
    {
        QRgb *row_dst = data_dst + (y*w);
        QRgb *row_src = data_src + (src_w*(y+radius));

        for (int x=0; x < w; x++)
        {
            float r=0, g=0, b=0;
            for (int i=0; i < width; i++)
            {
                QRgb clr = row_src[x+i];
                r += normal_kernel[i] * qRed(clr);
                g += normal_kernel[i] * qGreen(clr);
                b += normal_kernel[i] * qBlue(clr);
            }
            row_dst[x] = qRgba(round(r), round(g), round(b), qAlpha(row_dst[x]));
        }
    }
    // Convolve from top to bottom
    src_img = expandBorder(img, radius);
    data_src = (QRgb*)src_img.constScanLine(0);

    #pragma omp parallel for
    for (int y=0; y < h; y++)
    {
        QRgb *row_dst = data_dst + (y*w);

        for (int x=0; x < w; x++)
        {
            float r=0, g=0, b=0;
            for (int i=0; i < width; i++)
            {
                QRgb *row_src = data_src + (src_w*(y+i));
                QRgb clr = row_src[x+radius];
                r += normal_kernel[i] * qRed(clr);
                g += normal_kernel[i] * qGreen(clr);
                b += normal_kernel[i] * qBlue(clr);
            }
            row_dst[x] = qRgba( round(r), round(g), round(b), qAlpha(row_dst[x]) );
        }
    }
}

//*************---------- Gaussian Blur ---------***************//
// 1D Gaussian kernel -> g(x)   = 1/{sqrt(2.pi)*sigma} * e^{-(x^2)/(2.sigma^2)}
// 2D Gaussian kernel -> g(x,y) = 1/(2.pi.sigma^2) * e^{-(x^2 +y^2)/(2.sigma^2)}

#define PI 3.141593f

void gaussianBlur(QImage &img, int radius, float sigma/*standard deviation*/)
{
    if (sigma==0)  sigma = radius/2.0 ;
    int kernel_width = 2*radius + 1;
    // build 1D gaussian kernel
    float kernel[kernel_width];

    for (int i=0; i<kernel_width; i++)
    {
        int u = i - radius;
        double alpha = exp(-(u*u)/(2.0*sigma*sigma));
        kernel[i] = alpha/(sqrt(2*PI)*sigma);
    }
    convolve1D(img, kernel, kernel_width);
}

void kuwaharaFilter(QImage &img, int radius)
{
    int w = img.width();
    int h = img.height();
    QImage gaussImg = img.copy();
    gaussianBlur(gaussImg, radius, 0);
    int width = radius+1;

    QRgb *srcData = (QRgb*)gaussImg.constScanLine(0);
    QRgb *dstData = (QRgb*)img.scanLine(0);
    #pragma omp parallel for
    for (int y=0; y<h; y++)
    {
        for (int x=0; x<w; x++)
        {
            double min_variance = 1.7e308;//maximum for double
            RectInfo quadrant;
            RectInfo target = {0,0,1,1};
            for (int i=0; i<4; i++)
            {
                quadrant.x = x;
                quadrant.y = y;
                quadrant.width=width;
                quadrant.height=width;

                switch (i)
                {
                  case 0:
                  {
                    quadrant.x = x-(width-1);
                    quadrant.y = y-(width-1);
                    break;
                  }
                  case 1:
                  {
                    quadrant.y = y-(width-1);
                    break;
                  }
                  case 2:
                  {
                    quadrant.x = x-(width-1);
                    break;
                  }
                  default:
                    break;
                } // end of switch
                // manage boundary problem
                if (quadrant.x <0) {
                    quadrant.x=0;
                    quadrant.width = x+1;
                }
                else if (quadrant.x+quadrant.width>w)
                    quadrant.width = w-quadrant.x;
                if (quadrant.y <0) {
                    quadrant.y=0;
                    quadrant.height = y+1;
                }
                else if (quadrant.y+quadrant.height>h)
                    quadrant.height = h-quadrant.y;
                // calculate mean of variance
                double mean_r = 0, mean_g = 0, mean_b = 0;
                QRgb *quadRow = srcData + (w*quadrant.y + quadrant.x);
                // for each pixel in quadrant
                for (int m=0; m<quadrant.height; m++)
                {
                    for (int n=0; n<quadrant.width; n++) {
                      mean_r += (double) qRed(quadRow[n]);
                      mean_g += (double) qGreen(quadRow[n]);
                      mean_b += (double) qBlue(quadRow[n]);
                    }
                    quadRow += w;
                }
                mean_r /= (quadrant.width*quadrant.height);
                mean_g /= (quadrant.width*quadrant.height);
                mean_b /= (quadrant.width*quadrant.height);

                double mean_luma = getPixelLuma(mean_r, mean_g, mean_b);
                double variance=0.0;
                quadRow = srcData + (w*quadrant.y + quadrant.x);
                for (int m=0; m<quadrant.height; m++)
                {
                    for (int n=0; n<quadrant.width; n++) {
                      double luma = getPixelLuma(quadRow[n]);
                      variance += (luma-mean_luma)*(luma-mean_luma);
                    }
                    quadRow += w;
                }
                if (variance < min_variance)
                {
                    min_variance=variance;
                    target=quadrant;
                }
            }   // end quadrant loop
            QRgb clr = (srcData + (w*(target.y+target.height/2)))[(target.x+target.width/2)];
            (dstData + w*y)[x] = clr;
        }   // end column loop
    } // end row loop
}


QString FilterPlugin:: menuItem()
{
    // if you need / in menu name, use % character. Because / is path separator here
    return QString(PLUGIN_MENU);
}

void FilterPlugin:: onMenuClick()
{
    bool ok;
    int radius = QInputDialog::getInt(data->window, "Blur Radius", "Enter Blur Radius :",
                                        3/*val*/, 1/*min*/, 50/*max*/, 1/*step*/, &ok);
    if (not ok) return;
    kuwaharaFilter(data->image, radius);
    emit imageChanged();
}
