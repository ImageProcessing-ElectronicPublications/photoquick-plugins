#include "unalpha.h"

#define PLUGIN_NAME "UnAlpha"
#define PLUGIN_MENU "Filters/Color/UnAlpha"
#define PLUGIN_VERSION "1.0"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(unalpha, FilterPlugin);
#endif

// ********************** Unalpha *********************

void unalpha(QImage &img)
{
    int x, y, r, g, b, a;
    int imgW = img.width();
    int imgH = img.height();
    // Calc Histogram
    long long mr = 0, mg = 0 , mb = 0, n = 0, n2;
    for (y = 0; y < imgH; y++)
    {
        QRgb *row = (QRgb*)img.constScanLine(y);
        for (x = 0; x < imgW; x++)
        {
            r = qRed(row[x]);
            g = qGreen(row[x]);
            b = qBlue(row[x]);
            a = qAlpha(row[x]);
            mr += r * a;
            mg += g * a;
            mb += b * a;
            n += a;
        }
    }
    if (n > 0)
    {
        n2 = n / 2;
        mr += n2;
        mg += n2;
        mb += n2;
        mr /= n;
        mg /= n;
        mb /= n;
    } else {
        mr = 255;
        mg = 255;
        mb = 255;
    }
    for (y = 0; y < imgH; y++)
    {
        QRgb *row = (QRgb*)img.constScanLine(y);
        for (x = 0; x < imgW; x++)
        {
            r = qRed(row[x]);
            g = qGreen(row[x]);
            b = qBlue(row[x]);
            a = qAlpha(row[x]);
            r = (a * r + (255 - a) * (int)mr) / 255;
            g = (a * g + (255 - a) * (int)mg) / 255;
            b = (a * b + (255 - a) * (int)mb) / 255;
            r = Clamp(r);
            g = Clamp(g);
            b = Clamp(b);
            row[x] = qRgb(r, g, b);
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
    unalpha(data->image);
    emit imageChanged();
}
