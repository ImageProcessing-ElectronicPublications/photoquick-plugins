#include "ris_scaler.h"

#define PLUGIN_NAME "RIS Scalers"
#define PLUGIN_MENU "Transform/RIS"
#define PLUGIN_VERSION "1.0"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(ris-scaler, FilterPlugin);
#endif

void FilterPlugin:: filterScalerX(int n/*factor*/, int scaler)
{
    int w, h, w2, h2;
    w = data->image.width();
    h = data->image.height();
    if (scaler == SCALER_MEAN)
    {
        w2 = (w + n - 1) / n;
        h2 = (h + n - 1) / n;
    }
    else
    {
        w2 = w * n;
        h2 = h * n;
    }
    void *src = data->image.bits();
    QImage dstImg(w2, h2, data->image.format());
    void *dst = dstImg.bits();
    switch(scaler)
    {
        case SCALER_GSAMPLE:
            scaler_hris((uint*)src, (uint*)dst, w, h, n);
            break;
        case SCALER_HRIS:
            gsample((uint*)src, (uint*)dst, w, h, n);
            break;
        case SCALER_MEAN:
            scaler_mean_x((uint*)src, (uint*)dst, w, h, n);
            break;
    }
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
        filterScalerX(mult, method);
    }
}
