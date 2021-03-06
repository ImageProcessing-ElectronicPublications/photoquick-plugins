#include "dither.h"

#define PLUGIN_NAME "Dither"
#define PLUGIN_MENU "Filters/Threshold/Dither"
#define PLUGIN_VERSION "1.0"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(dither_thresh, FilterPlugin);
#endif

// **** Dither of D.E.Knuth "Computer Typesetting" ****
void dither(QImage &img, unsigned tcount, int tdelta, int kpg)
{
    unsigned x, y, d, i, j, k, l, lmin = 0;
    unsigned whg, wwn, iy0, ix0, iy, ix, ww;
    int tx, imm;
    // Knuth D.E. dither matrix
    int hdith[4][4] = {
        {  1,  5, 10, 14 },
        {  3,  7,  8, 12 },
        { 13,  9,  6,  2 },
        { 15, 11,  4,  0 }
    };
    int tdith[4][4] = {
        { 1, 5, 7, 0 },
        { 3, 6, 2, 0 },
        { 8, 4, 0, 0 },
        { 0, 0, 0, 0 }
    };
    int qdith[4][4] = {
        { 1, 2, 0, 0 },
        { 3, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 }
    };
    int dith[4][4], pix[4][4][3];
    int hdithy[2][17], hdithx[2][17], herr, herrp, herrg, herrmin;
    tcount = (tcount < 2) ? 2 : tcount;
    tcount = (tcount > 4) ? 4 : tcount;
    ww = tcount * tcount;
    unsigned imgW = img.width();
    unsigned imgH = img.height();

    if (tcount == 2)
    {
        for (y = 0; y < tcount; y++)
        {
            for (x = 0; x < tcount; x++)
            {
                dith[y][x] = qdith[y][x];
            }
        }
    }
    else if (tcount == 3)
    {
        for (y = 0; y < tcount; y++)
        {
            for (x = 0; x < tcount; x++)
            {
                dith[y][x] = tdith[y][x];
            }
        }
    }
    else
    {
        for (y = 0; y < tcount; y++)
        {
            for (x = 0; x < tcount; x++)
            {
                dith[y][x] = hdith[y][x];
            }
        }
    }
    for (y = 0; y < tcount; y++)
    {
        for (x = 0; x < tcount; x++)
        {
            l = dith[y][x] + 1;
            hdithy[0][l] = y;
            hdithx[0][l] = x;
            hdithy[1][l] = y;
            hdithx[1][l] = tcount - x - 1;
        }
    }

    whg = (imgH + tcount - 1) / tcount;
    wwn = (imgW + tcount - 1) / tcount;

    for (y = 0; y < whg; y++)
    {
        iy0 = y * tcount;
        for (x = 0; x < wwn; x++)
        {
            ix0 = x * tcount;
            for (i = 0; i < tcount; i++)
            {
                for (j = 0; j < tcount; j++)
                {
                    iy = iy0 + j;
                    ix = ix0 + i;
                    if (iy < imgH && ix < imgW)
                    {
                        QRgb *row = (QRgb*)img.constScanLine(iy);
                        pix[i][j][0] = qRed(row[ix]);
                        pix[i][j][1] = qGreen(row[ix]);
                        pix[i][j][2] = qBlue(row[ix]);
                    } else {
                        pix[i][j][0] = 255;
                        pix[i][j][1] = 255;
                        pix[i][j][2] = 255;
                    }
                }
            }
            k = (y + x) % 2;
            for (d = 0; d < 3; d++)
            {
                imm = 0;
                for (l = 1; l < (ww + 1); l++)
                {
                    j = hdithy[k][l];
                    i = hdithx[k][l];
                    tx = pix[i][j][d];
                    tx += tdelta;
                    imm += tx;
                }
                imm /= ww;
                herrp = herrg = 0;
                for (l = 1; l < (ww + 1); l++)
                {
                    j = hdithy[k][l];
                    i = hdithx[k][l];
                    tx = pix[i][j][d];
                    tx += tdelta;
                    herrg += (255 - tx);
                    tx -= imm;
                    tx *= kpg;
                    tx += imm;
                    tx = Clamp(tx);
                    herrp += (255 - tx);
                }
                herrmin = herrp + herrg;
                lmin = 0;
                for (l = 1; l < (ww + 1); l++)
                {
                    j = hdithy[k][l];
                    i = hdithx[k][l];
                    tx = pix[i][j][d];
                    tx += tdelta;
                    tx -= imm;
                    tx *= kpg;
                    tx += imm;
                    tx = Clamp(tx);
                    herrp += (tx + tx - 255);
                    herrg -= 255;
                    herr = (herrp < 0) ? (-herrp) : herrp;
                    herr += (herrg < 0) ? (-herrg) : herrg;
                    if (herr < herrmin)
                    {
                        herrmin = herr;
                        lmin = l;
                    }
                }
                for (l = 1; l < (ww + 1); l++)
                {
                    j = hdithy[k][l];
                    i = hdithx[k][l];
                    pix[i][j][d] = ( l > lmin ) ? 255 : 0;
                }
            }
            for (i = 0; i < tcount; i++)
            {
                ix = ix0 + i;
                for (j = 0; j < tcount; j++)
                {
                    iy = iy0 + j;
                    if (iy < imgH && ix < imgW)
                    {
                        QRgb *row = (QRgb*)img.constScanLine(iy);
                        row[ix] = qRgba(pix[i][j][0], pix[i][j][1], pix[i][j][2], qAlpha(row[ix]));
                    }
                }
            }
        }
    }
}

// **************** Dither Dialog ******************
DitherDialog:: DitherDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    gridLayout = new QGridLayout(this);

    labelcount = new QLabel("Pattern :", this);
    gridLayout->addWidget(labelcount, 0, 0, 1, 1);

    countSpin = new QSpinBox(this);
    countSpin->setAlignment(Qt::AlignCenter);
    countSpin->setRange(2, 4);
    gridLayout->addWidget(countSpin, 0, 1, 1, 1);

    labeldelta = new QLabel("Delta :", this);
    gridLayout->addWidget(labeldelta, 1, 0, 1, 1);

    deltaSpin = new QSpinBox(this);
    deltaSpin->setAlignment(Qt::AlignCenter);
    deltaSpin->setRange(0,255);
    deltaSpin->setValue(0);
    gridLayout->addWidget(deltaSpin, 1, 1, 1, 1);

    labelmult = new QLabel("Multiply :", this);
    gridLayout->addWidget(labelmult, 2, 0, 1, 1);

    kpgSpin = new QSpinBox(this);
    kpgSpin->setAlignment(Qt::AlignCenter);
    kpgSpin->setRange(0,255);
    kpgSpin->setValue(2);
    gridLayout->addWidget(kpgSpin, 2, 1, 1, 1);

    buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
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
    DitherDialog *dlg = new DitherDialog(data->window);
    if (dlg->exec()==QDialog::Accepted) {
        unsigned count = dlg->countSpin->value();
        int delta = dlg->deltaSpin->value();
        int kpg = dlg->kpgSpin->value();
        dither(data->image, count, delta, kpg);
        emit imageChanged();
    }
}
