#include "pixart_scaler.h"

#define PLUGIN_NAME "Pixart Scalers"
#define PLUGIN_VERSION "1.1"

Q_EXPORT_PLUGIN2(pixart-scaler, FilterPlugin);

QStringList
FilterPlugin:: menuItems()
{
    QStringList menu = {
        "Transform/Upscale Icon/Scale2x", "Transform/Upscale Icon/Scale3x",
        "Transform/Upscale Icon/Scale4x", "Transform/Upscale Icon/xBr 2x",
        "Transform/Upscale Icon/xBr 3x", "Transform/Upscale Icon/xBr 4x"};
    return menu;
}

void
FilterPlugin:: handleAction(QAction *action, int)
{
    if (action->text() == QString("Scale2x"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterScale2x()));
    else if (action->text() == QString("Scale3x"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterScale3x()));
    else if (action->text() == QString("Scale4x"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterScale4x()));
    else if (action->text() == QString("xBr 2x"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterXBR2x()));
    else if (action->text() == QString("xBr 3x"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterXBR3x()));
    else if (action->text() == QString("xBr 4x"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterXBR4x()));
}

void (*scaler_scalex_func[3])(uint*,uint*,int,int) = {
                scaler_scalex_2x, scaler_scalex_3x, scaler_scalex_4x};

void
FilterPlugin:: filterScaleX(int n/*factor*/)
{
    int w = canvas->image.width();
    int h = canvas->image.height();
    void *src = canvas->image.bits();
    QImage dstImg(n*w, n*h, canvas->image.format());
    void *dst = dstImg.bits();
    scaler_scalex_func[n-2]((uint*)src, (uint*)dst, w, h);
    canvas->image = dstImg;
    emit imageChanged();
}

void (*xbr_filter_func[3])(uint*,uint*,int,int) = {
                xbr_filter_xbr2x, xbr_filter_xbr3x, xbr_filter_xbr4x};

void
FilterPlugin:: filterXBR(int n/*factor*/)
{
    int w = canvas->image.width();
    int h = canvas->image.height();
    void *src = canvas->image.bits();
    QImage dstImg(n*w, n*h, canvas->image.format());
    void *dst = dstImg.bits();
    xbr_filter_func[n-2]((uint*)src, (uint*)dst, w, h);
    canvas->image = dstImg;
    emit imageChanged();
}

void
FilterPlugin:: filterScale2x()
{
    filterScaleX(2);
}

void
FilterPlugin:: filterScale3x()
{
    filterScaleX(3);
}

void
FilterPlugin:: filterScale4x()
{
    filterScaleX(4);
}

void
FilterPlugin:: filterXBR2x()
{
    filterXBR(2);
}

void
FilterPlugin:: filterXBR3x()
{
    filterXBR(3);
}

void
FilterPlugin:: filterXBR4x()
{
    filterXBR(4);
}

