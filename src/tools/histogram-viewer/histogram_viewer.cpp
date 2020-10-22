/*  This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed
    Copyright (C) 2020 Arindam Chaudhuri <ksharindam@gmail.com>
*/
#include "histogram_viewer.h"
#include <cmath>
#include <QPainter>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>


#define PLUGIN_NAME "Histogram Viewer"
#define PLUGIN_MENU "Tools/Histogram Viewer"
#define PLUGIN_VERSION "1.0"

Q_EXPORT_PLUGIN2(histogram-viewer, ToolPlugin);

//********* ---------- Histogram Viewer --------- ********** //

void getHistogram(QImage &img, uint hist_r[], uint hist_g[], uint hist_b[], uint hist_y[])
{
    int w = img.width();
    int h = img.height();

    for (int y=0; y<h; y++) {
        QRgb *row = (QRgb*) img.constScanLine(y);
        for (int x=0; x<w; x++) {
            int r = qRed(row[x]);
            int g = qGreen(row[x]);
            int b = qBlue(row[x]);
            int Y = r*0.2126f + g*0.7152f + b*0.0722f;
            ++hist_r[r];
            ++hist_g[g];
            ++hist_b[b];
            ++hist_y[Y];
        }
    }
}

template<typename T>
QImage histogramImage(T histogram[], QColor color, int hist_w, int hist_h)
{
    // get maximum in each histogram
    T max_val=0;
    for (int i=0; i<256; i++) {
        if (histogram[i] > max_val) max_val = histogram[i];
    }
    // create histogram image and draw histogram
    double factor = (double)hist_h / max_val;

    QImage hist_img(hist_w+2, hist_h+2, QImage::Format_RGB32);
    hist_img.fill(Qt::white);

    QPainter painter(&hist_img);
    // draw outline and grid
    painter.drawRect(0,0, hist_w+1, hist_h+1);
    painter.setPen(QColor(220,220,220));
    for (int i=1; i<5; i++) {
        painter.drawLine(QPoint(i*255/5,1), QPoint(i*255/5,hist_h));
    }
    // draw histogram bars
    painter.setPen(color);
    for (int i=0; i<256; i++) {
        int line_height = factor*histogram[i];
        if (line_height==0)
            continue;
        QPoint start(i+1, 1);
        QPoint end(i+1, line_height);
        painter.drawLine(start, end);
    }
    painter.end();
    // flip, because Y axis has reverse direction in QImage
    QTransform tfm;
    tfm.rotate(180, Qt::XAxis);
    return hist_img.transformed(tfm);
}


//--------- **************** Histogram Dialog ****************---------
HistogramDialog:: HistogramDialog(QWidget *parent, QImage &image) : QDialog(parent)
{
    this->setWindowTitle("R G B and Luminance(Y) Histogram");
    this->resize(540, 400);

    QGridLayout *gridLayout = new QGridLayout(this);

    histLabel = new QLabel(this);
    histLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    gridLayout->addWidget(histLabel, 0, 0, 1, 2);
    // container for holding buttons
    QWidget *container = new QWidget(this);
    gridLayout->addWidget(container, 1, 0, 1, 1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Close);
    gridLayout->addWidget(buttonBox, 1, 1, 1, 1);

    QHBoxLayout *hLayout = new QHBoxLayout(container);
    container->setLayout(hLayout);

    QCheckBox *logBtn = new QCheckBox("Logarithmic", this);
    hLayout->addWidget(logBtn);

    connect(logBtn, SIGNAL(clicked(bool)), this, SLOT(drawHistogram(bool)));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // generate histograms
    getHistogram(image, hist_r, hist_g, hist_b, hist_y);
    drawHistogram(false);
}

void
HistogramDialog:: drawHistogram(bool logarithmic)
{
    int hist_w = 256;
    int hist_h = 160;
    QImage imgR, imgG, imgB, imgY;
    if (not logarithmic) {
        imgR = histogramImage(hist_r, Qt::red, hist_w, hist_h);
        imgG = histogramImage(hist_g, Qt::green, hist_w, hist_h);
        imgB = histogramImage(hist_b, Qt::blue, hist_w, hist_h);
        imgY = histogramImage(hist_y, QColor(127,127,127), hist_w, hist_h);
    }
    else { // logarithmic histograms
        double loghist[4][256] = {};
        for (int i=0; i<256; i++) {
            loghist[0][i] = log(hist_r[i]);
            loghist[1][i] = log(hist_g[i]);
            loghist[2][i] = log(hist_b[i]);
            loghist[3][i] = log(hist_y[i]);
        }
        imgR = histogramImage(loghist[0], Qt::red, hist_w, hist_h);
        imgG = histogramImage(loghist[1], Qt::green, hist_w, hist_h);
        imgB = histogramImage(loghist[2], Qt::blue, hist_w, hist_h);
        imgY = histogramImage(loghist[3], QColor(127,127,127), hist_w, hist_h);
    }

    QImage img(hist_w*2+3, hist_h*2+3, QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter painter(&img);
    painter.drawImage(0, 0, imgR);
    painter.drawImage(hist_w+1, 0, imgG);
    painter.drawImage(0, hist_h+1, imgB);
    painter.drawImage(hist_w+1, hist_h+1, imgY);
    painter.end();

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
