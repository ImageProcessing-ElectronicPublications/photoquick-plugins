/*  This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed
    Copyright (C) 2020 Arindam Chaudhuri <ksharindam@gmail.com>
*/
#include "histogram_viewer.h"

#define PLUGIN_NAME "Histogram Viewer"
#define PLUGIN_MENU "Info/Histogram Viewer"
#define PLUGIN_VERSION "1.0"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(histogram-viewer, ToolPlugin);
#endif

//********* ---------- Histogram Viewer --------- ********** //

void getHistogram(QImage &img, uint hist_r[], uint hist_g[], uint hist_b[])
{
    int w = img.width();
    int h = img.height();
    int x, y, r, g, b;

    for (y = 0; y < h; y++)
    {
        QRgb *row = (QRgb*) img.constScanLine(y);
        for (x=0; x<w; x++)
        {
            r = qRed(row[x]);
            g = qGreen(row[x]);
            b = qBlue(row[x]);
            ++hist_r[r];
            ++hist_g[g];
            ++hist_b[b];
        }
    }
}

template<typename T> QImage histogramImage(T histr[], T histg[], T histb[], int hist_w, int hist_h, bool logarithmic)
{
    // get maximum in each histogram
    T max_val = 0;
    int i, j, i1, x, ro, go, bo;
    float factorh, factorl, factorw;
    float r, g, b, k, ix, kx;
    for (i = 0; i < 256; i++)
    {
        if (histr[i] > max_val) max_val = histr[i];
        if (histg[i] > max_val) max_val = histg[i];
        if (histb[i] > max_val) max_val = histb[i];
    }
    // create histogram image and draw histogram
    factorh = (float)max_val / (float)hist_h;
    factorl = (max_val > 0) ? (float)max_val / log((float)max_val + 1.0f) : 1.0f;
    factorw = 256.0f / (float)hist_w;

    QImage hist_img(hist_w, hist_h, QImage::Format_RGB32);

    // draw histogram bars
    for (j = 0; j < hist_h; j++)
    {
        QRgb *row = (QRgb*)hist_img.constScanLine(j);
        for (x = 0; x < hist_w; x++)
        {
            ix = factorw * x;
            i = (int)ix;
            i1 = ((i + 1) < hist_w) ? (i + 1) : i;
            kx = ix - i;
            k = (float)max_val - (float)j * factorh;
            r = ((1.0f - kx) * (float)histr[i] + kx * (float)histr[i1]);
            g = ((1.0f - kx) * (float)histg[i] + kx * (float)histg[i1]);
            b = ((1.0f - kx) * (float)histb[i] + kx * (float)histb[i1]);
            if (logarithmic)
            {
                r = log(r + 1.0f) * factorl;
                g = log(g + 1.0f) * factorl;
                b = log(b + 1.0f) * factorl;
            }
            ro = (r > k) ? 255 : 0;
            go = (g > k) ? 255 : 0;
            bo = (b > k) ? 255 : 0;
            row[x] = qRgb(ro, go, bo);
        }
    }
    return hist_img;
}


//--------- **************** Histogram Dialog ****************---------
HistogramDialog:: HistogramDialog(QWidget *parent, QImage &image) : QDialog(parent)
{
    this->setWindowTitle("R G B Histogram");
    this->resize(540, 400);

    gridLayout = new QGridLayout(this);

    histLabel = new QLabel(this);
    histLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    gridLayout->addWidget(histLabel, 0, 0, 1, 2);
    // container for holding buttons
    container = new QWidget(this);
    gridLayout->addWidget(container, 1, 0, 1, 1);

    buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Close);
    gridLayout->addWidget(buttonBox, 1, 1, 1, 1);

    hLayout = new QHBoxLayout(container);
    container->setLayout(hLayout);

    logBtn = new QCheckBox("Logarithmic", this);
    hLayout->addWidget(logBtn);

    connect(logBtn, SIGNAL(clicked(bool)), this, SLOT(drawHistogram(bool)));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // generate histograms
    getHistogram(image, hist_r, hist_g, hist_b);
    drawHistogram(false);
}

void HistogramDialog:: drawHistogram(bool logarithmic)
{
    int hist_w = 512;
    int hist_h = 320;
    QImage img;
    img = histogramImage(hist_r, hist_g, hist_b, hist_w, hist_h, logarithmic);

    histLabel->setPixmap(QPixmap::fromImage(img));
}

// ********* ----------- Plugin ---------- ********* //

QString ToolPlugin:: menuItem()
{
    return QString(PLUGIN_MENU);
}

void ToolPlugin:: onMenuClick()
{
    HistogramDialog *dlg = new HistogramDialog(data->window, data->image);
    dlg->exec();
}
