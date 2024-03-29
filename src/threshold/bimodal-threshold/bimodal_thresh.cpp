#include "bimodal_thresh.h"

#define PLUGIN_NAME "Bimodal Threshold"
#define PLUGIN_MENU "Filters/Threshold/Bimodal"
#define PLUGIN_VERSION "1.1"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(bimodal_thresh, FilterPlugin);
#endif

// ********************** Bimodal Threshold *********************
int histogram_darkest(long long hist[])
{
    for (int i=0; i<256; i++)
    {
        if (hist[i]!=0)
            return i;
    }
    return 255;
}
int histogram_lightest(long long hist[])
{
    for (int i=255; i>0; i--)
    {
        if (hist[i]!=0)
            return i;
    }
    return 0;
}

float threshold_bimod(long long hist[], int tsize, float tpart)
{
    int c;
    long long Tb, Tw, ib, iw;
    float tpartrev = 1.0 - tpart;
    int T = (int)(tsize * tpart + 0.5), Tn = 0;

    while (T != Tn)
    {
        Tn = T;
        Tb = 0;
        ib = 0;
        for (c = 0; c < T; c++)
        {
            Tb += hist[c] * c;
            ib += hist[c];
        }
        Tw=0;
        iw=0;
        for (c = T; c < tsize; c++)
        {
            Tw += hist[c] * c;
            iw += hist[c];
        }
        if ((iw + ib) == 0)
            T = Tn;
        else if (iw == 0)
            T = Tb / ib;
        else if (ib == 0)
            T = Tw / iw;
        else
            T = (int)((Tw / iw) * tpart + (Tb / ib) * tpartrev);
    }
    return T;
}

// get threshold values for a single channel
void thresholdBimodChannel(long long hist[], int thresval[], int tcount, int tdelta, bool median)
{
    int t, tt;
    long long sh, sht;
    int thres[256] = {-1}; //tcount+1 for c++11 compiler
    int newval[256] = {}; //tcount

    for (tt = 1; tt < tcount; tt++)
    {
        float part = (float)tt / tcount;
        thres[tt] = int(threshold_bimod(hist, 256, part) + 0.5 + tdelta);
    }

    if (median)
    {
        thres[0] = histogram_darkest(hist);
        thres[tcount] = histogram_lightest(hist); // get max
        for (tt = 0; tt < tcount; tt++)
        {
            sh = 0;
            sht = 0;
            for (t = thres[tt]; t <= thres[tt + 1]; t++)
            {
                sh += hist[t];
                sht += (hist[t] * t);
            }
            if (sh > 0)
            {
                newval[tt] = Clamp(sht / sh);
            } else {
                newval[tt] = Clamp((thres[tt] + thres[tt + 1]) / 2);
            }
        }
        thres[0] = -1;
    } else {
        for (tt = 0; tt < tcount; tt++)
        {
            newval[tt] = Clamp((255 * tt) / (tcount - 1));
        }
    }

    for (t = 0; t < 256; t++)
    {
        for (tt = 0; tt < tcount; tt++)
        {
            if (t > thres[tt])
                thresval[t] = newval[tt];
        }
    }
}

void thresholdBimod(QImage &img, int tcount, int tdelta, bool median)
{
    int imgW = img.width();
    int imgH = img.height();
    // Calc Histogram
    long long hist[3][256] = {};
    int thresval[3][256] = {};
    for (int y = 0; y < imgH; y++)
    {
        QRgb *row = (QRgb*)img.constScanLine(y);
        for (int x = 0; x < imgW; x++) {
            ++hist[0][qRed(row[x])];
            ++hist[1][qGreen(row[x])];
            ++hist[2][qBlue(row[x])];
        }
    }

    #pragma omp parallel for
    for (int i = 0; i < 3; i++)
    {
        thresholdBimodChannel(hist[i], thresval[i], tcount, tdelta, median);
    }
    // apply threshold to each pixel
    for (int y = 0; y < imgH; y++)
    {
        QRgb *row = (QRgb*)img.constScanLine(y);
        for (int x = 0; x < imgW; x++)
        {
            int clr = row[x];
            int r = thresval[0][qRed(clr)];
            int g = thresval[1][qGreen(clr)];
            int b = thresval[2][qBlue(clr)];
            row[x] = qRgba(r,g,b, qAlpha(clr));
        }
    }
}

// **************** Bimodal Threshold Dialog ******************
BimodThreshDialog:: BimodThreshDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    gridLayout = new QGridLayout(this);

    countLabel = new QLabel("Colors Count :", this);
    gridLayout->addWidget(countLabel, 0, 0, 1, 1);

    countSpin = new QSpinBox(this);
    countSpin->setAlignment(Qt::AlignCenter);
    countSpin->setRange(2, 255);
    gridLayout->addWidget(countSpin, 0, 1, 1, 1);

    deltaLabel = new QLabel("Delta :", this);
    gridLayout->addWidget(deltaLabel, 1, 0, 1, 1);

    deltaSpin = new QSpinBox(this);
    deltaSpin->setAlignment(Qt::AlignCenter);
    deltaSpin->setRange(-127,127);
    deltaSpin->setValue(0);
    gridLayout->addWidget(deltaSpin, 1, 1, 1, 1);

    medianBtn = new QCheckBox("Use Median", this);
    gridLayout->addWidget(medianBtn, 2, 0, 1, 1);

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
    BimodThreshDialog *dlg = new BimodThreshDialog(data->window);
    if (dlg->exec()==QDialog::Accepted)
    {
        int count = dlg->countSpin->value();
        int delta = dlg->deltaSpin->value();
        thresholdBimod(data->image, count, delta, dlg->medianBtn->isChecked());
        emit imageChanged();
    }
}
