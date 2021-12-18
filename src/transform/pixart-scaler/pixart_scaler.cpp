/*  This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed
    Copyright (C) 2020 Arindam Chaudhuri <ksharindam@gmail.com>
*/
#include "pixart_scaler.h"

#define PLUGIN_NAME "Pixart Scalers"
#define PLUGIN_MENU "Transform/Upscale Icon"
#define PLUGIN_VERSION "1.3"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(pixart-scaler, FilterPlugin);
#endif

void FilterPlugin:: UpcaleX(int method, int n/*factor*/)
{
    int w = data->image.width();
    int h = data->image.height();
    void *src = data->image.bits();
    QImage dstImg(n*w, n*h, data->image.format());
    void *dst = dstImg.bits();
    switch (method)
    {
    case 0:
        scaler_scalex((uint*)src, (uint*)dst, w, h, n);
        break;
    case 1:
        scaler_eagle((uint*)src, (uint*)dst, w, h, n);
        break;
    case 2:
        hqx((uint*)src, (uint*)dst, w, h, n);
        break;
    case 3:
        xbr_filter((uint*)src, (uint*)dst, w, h, n);
        break;
    default:
        return;
    }
    data->image = dstImg.copy();
    emit imageChanged();
}

// **************** Upscale Dialog ******************
UpscaleDialog:: UpscaleDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    gridLayout = new QGridLayout(this);

    labelMethod = new QLabel("Method :", this);
    gridLayout->addWidget(labelMethod, 0, 0, 1, 1);
    comboMethod = new QComboBox(this);
    comboMethod->addItems(itemsMethod);
    gridLayout->addWidget(comboMethod, 0, 1, 1, 1);

    labelMult = new QLabel("Mult :", this);
    gridLayout->addWidget(labelMult, 1, 0, 1, 1);
    spinMult = new QSpinBox(this);
    spinMult->setAlignment(Qt::AlignCenter);
    spinMult->setRange(2,4);
    spinMult->setValue(2);
    gridLayout->addWidget(spinMult, 1, 1, 1, 1);

    buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
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
        UpcaleX(method, mult);
    }
}
