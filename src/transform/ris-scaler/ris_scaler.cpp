#include "ris_scaler.h"

#define PLUGIN_NAME "RIS Scalers"
#define PLUGIN_MENU "Transform/RIS"
#define PLUGIN_VERSION "1.0"

Q_EXPORT_PLUGIN2(ris-scaler, FilterPlugin);

void FilterPlugin:: filterHRISX(int n/*factor*/)
{
    int w = data->image.width();
    int h = data->image.height();
    void *src = data->image.bits();
    QImage dstImg(n*w, n*h, data->image.format());
    void *dst = dstImg.bits();
    scaler_hris((uint*)src, (uint*)dst, w, h, n);
    data->image = dstImg.copy();
    emit imageChanged();
}

void FilterPlugin:: filterGSampleX(int n/*factor*/)
{
    int w = data->image.width();
    int h = data->image.height();
    void *src = data->image.bits();
    QImage dstImg(n*w, n*h, data->image.format());
    void *dst = dstImg.bits();
    gsample((uint*)src, (uint*)dst, w, h, n);
    data->image = dstImg.copy();
    emit imageChanged();
}

void FilterPlugin:: filterMeanX(int n/*factor*/)
{
    int w = data->image.width();
    int h = data->image.height();
    void *src = data->image.bits();
    QImage dstImg((w+n-1)/n, (h+n-1)/n, data->image.format());
    void *dst = dstImg.bits();
    scaler_mean_x((uint*)src, (uint*)dst, w, h, n);
    data->image = dstImg.copy();
    emit imageChanged();
}

// **************** RIS Dialog ******************
RISDialog:: RISDialog(QWidget *parent) : QDialog(parent)
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
    spinMult->setRange(2,3);
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
    RISDialog *dlg = new RISDialog(data->window);
    if (dlg->exec() == QDialog::Accepted)
    {
        int method = dlg->comboMethod->currentIndex();
        int mult = dlg->spinMult->value();
        switch (method)
        {
        case 0:
            filterGSampleX(mult);
            break;
        case 1:
            filterHRISX(mult);
            break;
        case 2:
            filterMeanX(mult);
            break;
        default:
            break;
        }
    }
}
