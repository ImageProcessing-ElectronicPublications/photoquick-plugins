/*  This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed
    Copyright (C) 2020 Arindam Chaudhuri <ksharindam@gmail.com>
*/
#include "stretch_histogram.h"

#define PLUGIN_NAME "Stretch Histogram"
#define PLUGIN_MENU "Filters/Color/Histogram Equalize"
#define PLUGIN_VERSION "1.0"

Q_EXPORT_PLUGIN2(stretch-histogram, FilterPlugin);

//*********** ------------ Stretch Histogram ----------- ************ //
/* This filter gives same effect as GIMP Colors->Auto->Equalize
   or GraphicsMagick  Enhance->Equalize */

// Stretch Histogram of a channel to 0-255 range
void stretchHistogramChannel(uint histogram[])
{
    // cumulative histogram
    uint cumu_hist[256];

    for (uint count=0, i=0; i < 256; i++)
    {
        count += histogram[i];
        cumu_hist[i] = count;
    }
    // Stretch the histogram based on cumulative histogram
    uint low  = cumu_hist[0];
    uint high = cumu_hist[255];

    if (low != high) {
        for (int i=0; i < 256; i++) {
            histogram[i] = 255 * (cumu_hist[i]-low)/(high-low);
        }
    }
}

void stretchHistogram(QImage &img)
{
    int w = img.width();
    int h = img.height();
    // Create Histogram
    uint histogram_r[256] = {};
    uint histogram_g[256] = {};
    uint histogram_b[256] = {};

    for (int y=0; y<h; y++) {
        QRgb *row = (QRgb*) img.constScanLine(y);
        for (int x=0; x<w; x++) {
            ++histogram_r[qRed(row[x])];
            ++histogram_g[qGreen(row[x])];
            ++histogram_b[qBlue(row[x])];
        }
    }
    // Stretch Histogram of three channels
    stretchHistogramChannel(histogram_r);
    stretchHistogramChannel(histogram_g);
    stretchHistogramChannel(histogram_b);
    // Apply Levels
    for (int y=0; y<h; y++) {
        QRgb *row = (QRgb*) img.scanLine(y);
        for (int x=0; x<w; x++) {
            int r = histogram_r[qRed(row[x])];
            int g = histogram_g[qGreen(row[x])];
            int b = histogram_b[qBlue(row[x])];
            row[x] = qRgba(r,g,b, qAlpha(row[x]));
        }
    }
}

// ************** ----------  Plugin Class -----------************* //

QString FilterPlugin:: menuItem()
{
    return QString(PLUGIN_MENU);
}

void FilterPlugin:: onMenuClick()
{
    stretchHistogram(data->image);
    emit imageChanged();
}
