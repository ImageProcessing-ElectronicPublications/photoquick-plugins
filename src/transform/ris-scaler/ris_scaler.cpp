#include "ris_scaler.h"
extern "C"
{
#include "libris.h"
}

#define PLUGIN_NAME "RIS Scalers"
#define PLUGIN_VERSION "1.0"

Q_EXPORT_PLUGIN2(ris-scaler, FilterPlugin);

QStringList FilterPlugin:: menuItems()
{
    QStringList menu = {
        "Transform/RIS/HRIS_2x", "Transform/RIS/HRIS_3x",
        "Transform/RIS/GSample_2x", "Transform/RIS/GSample_3x",
        "Transform/RIS/Mean_2r", "Transform/RIS/Mean_3r"};
    return menu;
}

void FilterPlugin:: handleAction(QAction *action, int)
{
    if (action->text() == QString("HRIS_2x"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterHRIS2x()));
    else if (action->text() == QString("HRIS_3x"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterHRIS3x()));
    else if (action->text() == QString("GSample_2x"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterGSample2x()));
    else if (action->text() == QString("GSample_3x"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterGSample3x()));
    else if (action->text() == QString("Mean_2r"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterMean2r()));
    else if (action->text() == QString("Mean_3r"))
        connect(action, SIGNAL(triggered()), this, SLOT(filterMean3r()));
}

void FilterPlugin:: filterHRISX(int n/*factor*/)
{
    int w = canvas->image.width();
    int h = canvas->image.height();
    void *src = canvas->image.bits();
    QImage dstImg(n*w, n*h, canvas->image.format());
    void *dst = dstImg.bits();
    scaler_hris((uint*)src, (uint*)dst, w, h, n);
    canvas->image = dstImg;
    emit imageChanged();
}

void FilterPlugin:: filterGSampleX(int n/*factor*/)
{
    int w = canvas->image.width();
    int h = canvas->image.height();
    void *src = canvas->image.bits();
    QImage dstImg(n*w, n*h, canvas->image.format());
    void *dst = dstImg.bits();
    gsample((uint*)src, (uint*)dst, w, h, n);
    canvas->image = dstImg;
    emit imageChanged();
}

void FilterPlugin:: filterMeanX(int n/*factor*/)
{
    int w = canvas->image.width();
    int h = canvas->image.height();
    void *src = canvas->image.bits();
    QImage dstImg((w+n-1)/n, (h+n-1)/n, canvas->image.format());
    void *dst = dstImg.bits();
    scaler_mean_x((uint*)src, (uint*)dst, w, h, n);
    canvas->image = dstImg;
    emit imageChanged();
}

void FilterPlugin:: filterHRIS2x()
{
    filterHRISX(2);
}

void FilterPlugin:: filterHRIS3x()
{
    filterHRISX(3);
}

void FilterPlugin:: filterGSample2x()
{
    filterGSampleX(2);
}

void FilterPlugin:: filterGSample3x()
{
    filterGSampleX(3);
}

void FilterPlugin:: filterMean2r()
{
    filterMeanX(2);
}

void FilterPlugin:: filterMean3r()
{
    filterMeanX(3);
}

