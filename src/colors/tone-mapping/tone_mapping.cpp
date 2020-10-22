/*  This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed
    Copyright (C) 2020 Arindam Chaudhuri <ksharindam@gmail.com>
*/
#include "tone_mapping.h"
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QDialogButtonBox>

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
    Mantiuk06Dialog *dlg = new Mantiuk06Dialog(data->window);
    if (dlg->exec()==QDialog::Accepted) {
        toneMapping_mantiuk06(data->image, dlg->contrastSpin->value(),
                                        dlg->saturationSpin->value());
        emit imageChanged();
    }
}

// **************** Mantiuk06 Dialog ******************
Mantiuk06Dialog:: Mantiuk06Dialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    QGridLayout *gridLayout = new QGridLayout(this);

    QLabel *label = new QLabel("Contrast :", this);
    gridLayout->addWidget(label, 0, 0, 1, 1);

    contrastSpin = new QDoubleSpinBox(this);
    contrastSpin->setAlignment(Qt::AlignCenter);
    contrastSpin->setSingleStep(0.05);
    contrastSpin->setRange(0.01, 1.0);
    contrastSpin->setValue(0.1);
    gridLayout->addWidget(contrastSpin, 0, 1, 1, 1);

    QLabel *label_2 = new QLabel("Saturation :", this);
    gridLayout->addWidget(label_2, 1, 0, 1, 1);

    saturationSpin = new QDoubleSpinBox(this);
    saturationSpin->setAlignment(Qt::AlignCenter);
    saturationSpin->setSingleStep(0.05);
    saturationSpin->setRange(0.01, 2.0);
    saturationSpin->setValue(0.8);
    gridLayout->addWidget(saturationSpin, 1, 1, 1, 1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    gridLayout->addWidget(buttonBox, 2, 0, 1, 2);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}
