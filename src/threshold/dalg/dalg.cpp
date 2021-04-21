#include "dalg.h"
#include <QLabel>
#include <QDialogButtonBox>
#include <QGridLayout>

#define PLUGIN_NAME "Dalg"
#define PLUGIN_MENU "Filters/Threshold/Dalg"
#define PLUGIN_VERSION "1.0"

// first parameter is name of plugin, usually same as the library file name
Q_EXPORT_PLUGIN2(dalg_thresh, FilterPlugin);

// clamp an integer in 0-255 range
#define Clamp(a) ( (a)&(~0xff) ? (uchar)((~a)>>31) : (a) )

// **** D-algoritm dither ****
void dalg(QImage &img, unsigned tcount, int tdelta)
{
    unsigned x, y, i, j, d, Tmax = 256;
    unsigned whg, wwn, iy0, ix0, iyn, ixn;
    unsigned wwidth = tcount;
    float sw[3], swt[3], dsr[3], dst[3];
    unsigned long long hist[3][256] = {0};
    int r, g, b, a, tt[3];
    unsigned width = img.width();
    unsigned height = img.height();

    whg = (height + wwidth - 1) / wwidth;
    wwn = (width + wwidth - 1) / wwidth;
    for (y = 0; y < whg; y++)
    {
        iy0 = y * wwidth;
        iyn = iy0 + wwidth;
        if (iyn > height) {iyn = height;}
        for (x = 0; x < wwn; x++)
        {
            ix0 = x * wwidth;
            ixn = ix0 + wwidth;
            if (ixn > width) {ixn = width;}
            for (d = 0; d < 3; d++)
                sw[d] = 0;
            for (j = iy0; j < iyn; j++)
            {
                QRgb *row = (QRgb*)img.constScanLine(j);
                for (i = ix0; i < ixn; i++)
                {
                    sw[0] += qRed(row[i]);
                    sw[1] += qGreen(row[i]);
                    sw[2] += qBlue(row[i]);
                }
            }
            for (d = 0; d < 3; d++)
            {
                swt[d] = 0;
                tt[d] = Tmax - 1;
                for (i = 0; i < Tmax; i++)
                    hist[d][i] = 0;
            }
            for (j = iy0; j < iyn; j++)
            {
                QRgb *row = (QRgb*)img.constScanLine(j);
                for (i = ix0; i < ixn; i++)
                {
                    ++hist[0][qRed(row[i])];
                    ++hist[1][qGreen(row[i])];
                    ++hist[2][qBlue(row[i])];
                }
            }
            for (d = 0; d < 3; d++)
            {
                while ( swt[d] < sw[d] && tt[d] > 0)
                {
                    dsr[d] = sw[d] - swt[d];
                    swt[d] += (hist[d][tt[d]] * (Tmax - 1));
                    dst[d] = swt[d] - sw[d];
                    tt[d]--;
                }
                if (dst[d] > dsr[d])
                    tt[d]++;
                tt[d] += tdelta;
            }
            for (j = iy0; j < iyn; j++)
            {
                QRgb *row = (QRgb*)img.constScanLine(j);
                for (i = ix0; i < ixn; i++)
                {
                    r = qRed(row[i]);
                    g = qGreen(row[i]);
                    b = qBlue(row[i]);
                    a = qAlpha(row[i]);
                    r = ((r > tt[0]) ? 255 : 0);
                    g = ((g > tt[1]) ? 255 : 0);
                    b = ((b > tt[2]) ? 255 : 0);
                    row[i] = qRgba(r, g, b, a);
                }
            }
        }
    }
}

// **************** Dither Dialog ******************
DalgDialog:: DalgDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    QGridLayout *gridLayout = new QGridLayout(this);

    QLabel *labelcount = new QLabel("Pattern :", this);
    gridLayout->addWidget(labelcount, 0, 0, 1, 1);

    countSpin = new QSpinBox(this);
    countSpin->setAlignment(Qt::AlignCenter);
    countSpin->setRange(2, 256);
    countSpin->setValue(4);
    gridLayout->addWidget(countSpin, 0, 1, 1, 1);

    QLabel *labeldelta = new QLabel("Delta :", this);
    gridLayout->addWidget(labeldelta, 1, 0, 1, 1);

    deltaSpin = new QSpinBox(this);
    deltaSpin->setAlignment(Qt::AlignCenter);
    deltaSpin->setRange(0,255);
    deltaSpin->setValue(0);
    gridLayout->addWidget(deltaSpin, 1, 1, 1, 1);

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
    DalgDialog *dlg = new DalgDialog(data->window);
    if (dlg->exec()==QDialog::Accepted) {
        unsigned count = dlg->countSpin->value();
        int delta = dlg->deltaSpin->value();
        dalg(data->image, count, delta);
        emit imageChanged();
    }
}
