/*  This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed
    Copyright (C) 2020 Arindam Chaudhuri <ksharindam@gmail.com>
*/
#include "pixart_scaler.h"

#define PLUGIN_NAME "Pixart Scalers"
#define PLUGIN_MENU "Transform/Upscale Icon"
#define PLUGIN_VERSION "1.2"

Q_EXPORT_PLUGIN2(pixart-scaler, FilterPlugin);

void (*scaler_scalex_func[3])(uint*,uint*,int,int) = {
                scaler_scalex_2x, scaler_scalex_3x, scaler_scalex_4x};

void
FilterPlugin:: filterScaleX(int n/*factor*/)
{
    int w = data->image.width();
    int h = data->image.height();
    void *src = data->image.bits();
    QImage dstImg(n*w, n*h, data->image.format());
    void *dst = dstImg.bits();
    scaler_scalex_func[n-2]((uint*)src, (uint*)dst, w, h);
    data->image = dstImg;
    emit imageChanged();
}

void (*xbr_filter_func[3])(uint*,uint*,int,int) = {
                xbr_filter_xbr2x, xbr_filter_xbr3x, xbr_filter_xbr4x};

void
FilterPlugin:: filterXBR(int n/*factor*/)
{
    int w = data->image.width();
    int h = data->image.height();
    void *src = data->image.bits();
    QImage dstImg(n*w, n*h, data->image.format());
    void *dst = dstImg.bits();
    xbr_filter_func[n-2]((uint*)src, (uint*)dst, w, h);
    data->image = dstImg;
    emit imageChanged();
}

// **************** RIS Dialog ******************
UpscaleDialog:: UpscaleDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    QGridLayout *gridLayout = new QGridLayout(this);

    QLabel *labelMethod = new QLabel("Method :", this);
    gridLayout->addWidget(labelMethod, 0, 0, 1, 1);
    comboMethod = new QComboBox(this);
    QStringList items = { "ScaleX", "xBr"};
    comboMethod->addItems(items);
    gridLayout->addWidget(comboMethod, 0, 1, 1, 1);

    QLabel *labelMult = new QLabel("Mult :", this);
    gridLayout->addWidget(labelMult, 1, 0, 1, 1);
    spinMult = new QSpinBox(this);
    spinMult->setAlignment(Qt::AlignCenter);
    spinMult->setRange(2,4);
    spinMult->setValue(2);
    gridLayout->addWidget(spinMult, 1, 1, 1, 1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    gridLayout->addWidget(buttonBox, 2, 0, 1, 2);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

QString FilterPlugin:: menuItem()
{
    return QString(PLUGIN_MENU);
}

void FilterPlugin:: onMenuClick()
{
    UpscaleDialog *dlg = new UpscaleDialog(data->window);
    if (dlg->exec() == QDialog::Accepted)
    {
        int method = dlg->comboMethod->currentIndex();
        int mult = dlg->spinMult->value();
        switch (method)
        {
        case 0:
            filterScaleX(mult);
            break;
        case 1:
            filterXBR(mult);
            break;
        default:
            break;
        }
    }
}
