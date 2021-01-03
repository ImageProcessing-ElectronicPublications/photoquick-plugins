#include "threshold_bg_scale.h"
#include <QLabel>
#include <QDialogButtonBox>
#include <QGridLayout>

#define PLUGIN_NAME "Threshold BG Scale"
#define PLUGIN_MENU "Filters/Threshold/Threshold BG Scale"
#define PLUGIN_VERSION "1.0"

Q_EXPORT_PLUGIN2(threshold-bg-scale, FilterPlugin);

//***** ------ Threshold by difference from Blurred Background ----- ***** //
void thresholdBgScale(QImage &img, int thresh, int scaledW)
{
    int x, y, r, g, b, a;
    int imgW = img.width();
    int imgH = img.height();
    // first downscale then upscale to blur image
    QImage scaledImg = img.scaled(scaledW, scaledW, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    scaledImg = scaledImg.scaled(imgW, imgH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    for (y = 0; y < imgH; y++)
    {
        QRgb* line = (QRgb*) img.scanLine(y);
        QRgb* lineScaled = (QRgb*) scaledImg.constScanLine(y);
        for (x = 0; x < imgW; x++)
        {
            r = qRed(lineScaled[x]);
            r -= qRed(line[x]);
            g = qGreen(lineScaled[x]);
            g -= qGreen(line[x]);
            b = qBlue(lineScaled[x]);
            b -= qBlue(line[x]);
			a = qAlpha(line[x]);
            r = (r > thresh) ? 0 : 255;
            g = (g > thresh) ? 0 : 255;
            b = (b > thresh) ? 0 : 255;
            line[x] = qRgba(r, g, b, a);
        }
    }
}

// **************** Plugin Input Dialog ******************
PluginDialog:: PluginDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    QGridLayout *gridLayout = new QGridLayout(this);

    QLabel *label = new QLabel("Threshold (delta) :", this);
    gridLayout->addWidget(label, 0, 0, 1, 1);

    thresholdSpin = new QSpinBox(this);
    thresholdSpin->setAlignment(Qt::AlignCenter);
    thresholdSpin->setRange(-127, 127);
    gridLayout->addWidget(thresholdSpin, 0, 1, 1, 1);

    QLabel *label_2 = new QLabel("Downscale Size :", this);
    gridLayout->addWidget(label_2, 1, 0, 1, 1);

    downscaledSizeSpin = new QSpinBox(this);
    downscaledSizeSpin->setAlignment(Qt::AlignCenter);
    downscaledSizeSpin->setMinimum(1);
    downscaledSizeSpin->setValue(8);
    gridLayout->addWidget(downscaledSizeSpin, 1, 1, 1, 1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    gridLayout->addWidget(buttonBox, 2, 0, 1, 2);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

// ****************** Threshold BG Scale Plugin *******************
QString FilterPlugin:: menuItem()
{
    return QString(PLUGIN_MENU);
}

void FilterPlugin:: onMenuClick()
{
    PluginDialog *dlg = new PluginDialog(data->window);
    if (dlg->exec()==QDialog::Accepted) {
        int threshold = dlg->thresholdSpin->value();
        int scaledW = dlg->downscaledSizeSpin->value();
        thresholdBgScale(data->image, threshold, scaledW);
        emit imageChanged();
    }
}
