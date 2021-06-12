#include "invert.h"

#define PLUGIN_NAME "Invert%Negative"
#define PLUGIN_MENU "Filters/Color/Invert%Negative"
#define PLUGIN_VERSION "4.3.3"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(invert, FilterPlugin);
#endif

//********* ---------- Invert Colors or Negate --------- ********** //
void invert(QImage &img)
{
    for (int y=0;y<img.height();y++) {
        QRgb* line = (QRgb*) img.scanLine(y);
        for (int x=0;x<img.width();x++) {
            line[x] = qRgba(255-qRed(line[x]), 255-qGreen(line[x]), 255-qBlue(line[x]), qAlpha(line[x]));
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
    invert(data->image);
    emit imageChanged();
}
