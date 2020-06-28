#include "greyworld.h"
#include <cmath>

#define PLUGIN_NAME "GreyWorld"
#define PLUGIN_MENU "Filters/Color/GreyWorld"
#define PLUGIN_VERSION "4.3.3"

// first parameter is name of plugin, usually same as the library file name
Q_EXPORT_PLUGIN2(greyworld, FilterPlugin);

//********* ---------- GreyWorld filter --------- ********** //
// clamp an integer in 0-255 range
#define Clamp(a) ( (a)&(~0xff) ? (uchar)((~a)>>31) : (a) )
void greyworld(QImage &img)
{
    int y, x;
    long long mi = 0, mr = 0, mg = 0, mb = 0, n = 0, mn;
    float a0r = 0.0f, a0g = 0.0f, a0b = 0.0f;
    float a1r = 1.0f, a1g = 1.0f, a1b = 1.0f;
    float km = 0.001f / 3.0f, vm = 127.0f;
    int vr, vg, vb;
    QRgb* line;

    for (y = 0; y < img.height(); y++)
    {
        line = (QRgb*) img.scanLine(y);
        for (x = 0; x < img.width(); x++)
        {
            n++;
            mr += qRed(line[x]);
            mg += qGreen(line[x]);
            mb += qBlue(line[x]);
        }
    }
    mi = (mr + mg + mb) * 1000;
    mn = mi / n;
    vm = km * (float)mn;
    if (mr > 0)
    {
        mr = mi / mr;
        a1r = km * (float)mr;
    }
    else
    {
        a0r = vm;
    }
    if (mg > 0)
    {
        mg = mi / mg;
        a1g = km * (float)mg;
    }
    else
    {
        a0g = vm;
    }
    if (mb > 0)
    {
        mb = mi / mb;
        a1b = km * (float)mb;
    }
    else
    {
        a0b = vm;
    }
    for (y = 0; y < img.height(); y++)
    {
        line = (QRgb*) img.scanLine(y);
        for (x = 0; x < img.width(); x++)
        {
            vr = qRed(line[x]);
            vr = (int)(a1r * (float)vr + a0r);
            vr = Clamp(vr);
            vg = qGreen(line[x]);
            vg = (int)(a1g * (float)vg + a0g);
            vg = Clamp(vg);
            vb = qBlue(line[x]);
            vb = (int)(a1b * (float)vb + a0b);
            vb = Clamp(vb);
            line[x] = qRgba(vr, vg, vb, qAlpha(line[x]));
        }
    }
}

QString FilterPlugin:: menuItem()
{
    // if you need / in menu name, use % character. Because / is path separator here
    return QString(PLUGIN_MENU);
}

void FilterPlugin:: onMenuClick()
{
    greyworld(canvas->image);
    emit imageChanged();
}
