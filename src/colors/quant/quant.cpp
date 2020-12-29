#include "quant.h"
#include <QLabel>
#include <QDialogButtonBox>
#include <QGridLayout>

#define PLUGIN_NAME "Quant"
#define PLUGIN_MENU "Filters/Color/Quant"
#define PLUGIN_VERSION "1.0"

// first parameter is name of plugin, usually same as the library file name
Q_EXPORT_PLUGIN2(quant, FilterPlugin);

// ********************** Quant Simple *********************
// clamp an integer in 0-255 range
#define Clamp(a) ( (a)&(~0xff) ? (uchar)((~a)>>31) : (a) )

void Quant(QImage &img, int red, int green, int blue)
{
	int y, x, clr;
	int r, g, b;
    int imgW = img.width();
    int imgH = img.height();
	red = (red < 2) ? 2 : red;
	green = (green < 2) ? 2 : green;
	blue = (blue < 2) ? 2 : blue;
    for (y = 0; y < imgH; y++)
    {
        QRgb *row = (QRgb*)img.constScanLine(y);
        for (x = 0; x < imgW; x++)
		{
            clr = row[x];
            r = qRed(clr);
            g = qGreen(clr);
            b = qBlue(clr);
			r = (int)((float)r * red * 0.00390625);
			g = (int)((float)g * green * 0.00390625);
			b = (int)((float)b * blue * 0.00390625);
			r = (int)((float)r * 255.0 / (float)(red - 1.0));
			g = (int)((float)g * 255.0 / (float)(green - 1.0));
			b = (int)((float)b * 255.0 / (float)(blue - 1.0));
			r = Clamp(r);
			g = Clamp(g);
			b = Clamp(b);
            row[x] = qRgba(r, g, b, qAlpha(clr));
        }
    }
}

// **************** Bimodal Threshold Dialog ******************
QuantDialog:: QuantDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    QGridLayout *gridLayout = new QGridLayout(this);

    QLabel *redLabel = new QLabel("Red :", this);
    gridLayout->addWidget(redLabel, 0, 0, 1, 1);
    redSpin = new QSpinBox(this);
    redSpin->setAlignment(Qt::AlignCenter);
    redSpin->setRange(2, 256);
    redSpin->setValue(8);
    gridLayout->addWidget(redSpin, 0, 1, 1, 1);

    QLabel *greenLabel = new QLabel("Green :", this);
    gridLayout->addWidget(greenLabel, 1, 0, 1, 1);
    greenSpin = new QSpinBox(this);
    greenSpin->setAlignment(Qt::AlignCenter);
    greenSpin->setRange(2,256);
    greenSpin->setValue(8);
    gridLayout->addWidget(greenSpin, 1, 1, 1, 1);

    QLabel *blueLabel = new QLabel("Blue :", this);
    gridLayout->addWidget(blueLabel, 2, 0, 1, 1);
    blueSpin = new QSpinBox(this);
    blueSpin->setAlignment(Qt::AlignCenter);
    blueSpin->setRange(2,256);
    blueSpin->setValue(8);
    gridLayout->addWidget(blueSpin, 2, 1, 1, 1);


    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    gridLayout->addWidget(buttonBox, 3, 0, 1, 2);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

QString FilterPlugin:: menuItem()
{
    return QString(PLUGIN_MENU);
}

void FilterPlugin:: onMenuClick()
{
    QuantDialog *dlg = new QuantDialog(data->window);
    if (dlg->exec()==QDialog::Accepted) {
        int red = dlg->redSpin->value();
        int green = dlg->greenSpin->value();
        int blue = dlg->blueSpin->value();
        Quant(data->image, red, green, blue);
        emit imageChanged();
    }
}
