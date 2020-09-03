/* This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed */
#include "tone_mapping.h"
#include <QDebug>

#define PLUGIN_NAME "Tone Mapping"
#define PLUGIN_VERSION "1.0"

Q_EXPORT_PLUGIN2(tone-mapping, FilterPlugin);

void toneMapping_mantiuk06(QImage &img, float contrast=0.1, float saturation=0.8);

QString
FilterPlugin:: menuItem()
{
    return QString("Filters/HDR Effect");
}

void
FilterPlugin:: onMenuClick()
{
    toneMapping_mantiuk06(data->image, 0.1, 0.8);
    emit imageChanged();
}

