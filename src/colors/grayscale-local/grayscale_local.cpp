/*  This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed
    Copyright (C) 2020 Arindam Chaudhuri <ksharindam@gmail.com>
*/
/* Color to grayscale conversion, uses envelopes formed with the STRESS approach
    to perform local color-difference preserving grayscale generation.
*/
#include "grayscale_local.h"

#define PLUGIN_NAME "GrayScale (Local)"
#define PLUGIN_VERSION "1.0"

Q_EXPORT_PLUGIN2(grayscale-local, FilterPlugin);

QImage color2gray(QImage &image, int radius, int samples, int iterations, bool enhance_shadows);

QString
FilterPlugin:: menuItem()
{
    return QString("Filters/Color/GrayScale (Local)");
}

void
FilterPlugin:: onMenuClick()
{
    GrayScaleDialog *dlg = new GrayScaleDialog(data->window);
    if (dlg->exec()==QDialog::Accepted) {
        data->image = color2gray (data->image, dlg->radiusSpin->value(),
                                                dlg->samplesSpin->value(),
                                                dlg->iterationsSpin->value(),
                                                dlg->enhanceShadowsBtn->isChecked());
        emit imageChanged();
    }
}

// **************** Input Options Dialog ******************
// descriptions for tooltip
#define RADIUS_DESC "Neighborhood taken into account, this is the radius \n"\
                     "in pixels taken into account when deciding which \n"\
                     "colors map to which gray values"
#define SAMPLES_DESC "Number of samples to do per iteration looking for the range of colors"
#define ITERATIONS_DESC "Number of iterations, a higher number of iterations \n"\
                     "provides less noisy results at a computational cost"
#define ENHANCE_SHADOWS_DESC "When enabled details in shadows are boosted at the expense of noise"

GrayScaleDialog:: GrayScaleDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 184);

    gridLayout = new QGridLayout(this);

    radiusLabel = new QLabel("Radius :", this);
    gridLayout->addWidget(radiusLabel, 0, 0, 1, 1);

    radiusSpin = new QSpinBox(this);
    radiusSpin->setAlignment(Qt::AlignCenter);
    radiusSpin->setRange(2, 1000);
    radiusSpin->setValue(300);
    gridLayout->addWidget(radiusSpin, 0, 1, 1, 1);

    samplesLabel = new QLabel("Samples :", this);
    gridLayout->addWidget(samplesLabel, 1, 0, 1, 1);

    samplesSpin = new QSpinBox(this);
    samplesSpin->setAlignment(Qt::AlignCenter);
    samplesSpin->setRange(3, 17);
    samplesSpin->setValue(4);
    gridLayout->addWidget(samplesSpin, 1, 1, 1, 1);

    iterationsLabel = new QLabel("Iterations :", this);
    gridLayout->addWidget(iterationsLabel, 2, 0, 1, 1);

    iterationsSpin = new QSpinBox(this);
    iterationsSpin->setAlignment(Qt::AlignCenter);
    iterationsSpin->setRange(1, 30);
    iterationsSpin->setValue(10);
    gridLayout->addWidget(iterationsSpin, 2, 1, 1, 1);

    enhanceShadowsBtn = new QCheckBox("Enhance Shadows", this);
    gridLayout->addWidget(enhanceShadowsBtn, 3, 0, 1, 1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    gridLayout->addWidget(buttonBox, 4, 0, 1, 2);

    radiusSpin->setToolTip(RADIUS_DESC);
    samplesSpin->setToolTip(SAMPLES_DESC);
    iterationsSpin->setToolTip(ITERATIONS_DESC);
    enhanceShadowsBtn->setToolTip(ENHANCE_SHADOWS_DESC);
    radiusLabel->setToolTip(RADIUS_DESC);
    samplesLabel->setToolTip(SAMPLES_DESC);
    iterationsLabel->setToolTip(ITERATIONS_DESC);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}
