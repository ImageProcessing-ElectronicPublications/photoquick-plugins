#include "bimodal_adaptive.h"

#define PLUGIN_NAME "Bimodal adaptive"
#define PLUGIN_MENU "Filters/Threshold/Bimodal adaptive"
#define PLUGIN_VERSION "4.4.3"

// first parameter is name of plugin, usually same as the library file name
Q_EXPORT_PLUGIN2(adaptive_bimodal, FilterPlugin);

int SelectChannelPixel(QRgb pix, int channel)
{
    int value;
    switch (channel)
    {
     case 0:
        value = qRed(pix);
        break;
    case 1:
        value = qGreen(pix);
        break;
    case 2:
        value = qBlue(pix);
        break;
    default:
        value = qAlpha(pix);
        break;
    }
    return value;
}

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

QImage thresholdBimod(QImage &img, int tcount, int tdelta, bool median)
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
    QImage imgd = img.copy();
    for (int y = 0; y < imgH; y++)
    {
        QRgb *row = (QRgb*)img.constScanLine(y);
        QRgb *rowd = (QRgb*)imgd.constScanLine(y);
        for (int x = 0; x < imgW; x++)
        {
            int clr = row[x];
            int r = thresval[0][qRed(clr)];
            int g = thresval[1][qGreen(clr)];
            int b = thresval[2][qBlue(clr)];
            rowd[x] = qRgba(r,g,b, qAlpha(clr));
        }
    }
    return imgd;
}

//*********---------- Adaptive Threshold ---------**********//
// Apply Bradley threshold (to get desired output, tune value of T and s)
void thresholdAdaptBimod(QImage &img, float T, int window_size)
{
    QImage imgbm = thresholdBimod(img, 2, 0, false);
    int w = img.width();
    int h = img.height();
    // Allocate memory for integral image
    int len = sizeof(int *) * h + sizeof(int) * w * h;
    unsigned **intImg = (unsigned **)malloc(len);

    unsigned *ptr = (unsigned*)(intImg + h);
    for (int i = 0; i < h; i++)
        intImg[i] = (ptr + w * i);

    // Calculate integral image
    int dn = (img.hasAlphaChannel()) ? 4 : 3;
    for (int d = 0; d < dn; d++)
    {
        for (int y = 0; y < h; ++y)
        {
            QRgb *row = (QRgb*)img.constScanLine(y);
            int c, sum = 0;
            for (int x = 0; x < w; ++x)
            {
                c = SelectChannelPixel(row[x], d);
                sum += c;
                if (y == 0)
                    intImg[y][x] = c;
                else
                    intImg[y][x] = intImg[y-1][x] + sum;
            }
        }
        // Apply Bradley threshold
        window_size = (window_size > 0) ? window_size : MAX(16, w/32);
        int s2 = window_size / 2;
        for (int i = 0; i < h; ++i)
        {
            int x1,y1,x2,y2, count, sum;
            y1 = ((i - s2) > 0) ? (i - s2) : 0;
            y2 = ((i + s2) < h) ? (i + s2) : (h - 1);
            QRgb *row = (QRgb*)img.scanLine(i);
            QRgb *rowbm = (QRgb*)imgbm.scanLine(i);
            for (int j = 0; j < w; ++j)
            {
                x1 = ((j - s2) > 0) ? (j - s2) : 0;
                x2 = ((j + s2) < w) ? (j + s2) : (w - 1);

                count = (x2 - x1) * (y2 - y1);
                sum = intImg[y2][x2] - intImg[y2][x1] - intImg[y1][x2] + intImg[y1][x1];

                // threshold = mean * (1 - T) , where mean = sum / count, T = around 0.15
                int clr = row[j];
                int clrbm = rowbm[j];
                int ct, cs, c = SelectChannelPixel(clr, d);
                int bm = SelectChannelPixel(clrbm, d);
                int r = qRed(clr);
                int g = qGreen(clr);
                int b = qBlue(clr);
                int a = qAlpha(clr);
                float kT = (bm > 0) ? (1.0f - T) : (1.0f + T);
                cs = (int)(sum * kT + 0.5f);
                ct = ((c * count) > cs) ? 255: 0;
                switch (d)
                {
                 case 0:
                    r = ct;
                    break;
                case 1:
                    g = ct;
                    break;
                case 2:
                    b = ct;
                    break;
                default:
                    a = ct;
                    break;
                }
                row[j] = (dn > 3) ? qRgba(r, g, b, a) : qRgb(r, g, b);
            }
        }
    }
    free(intImg);
}

// **************** Adaptive Bimodal Threshold Dialog ******************
AdaptBimodThreshDialog:: AdaptBimodThreshDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    gridLayout = new QGridLayout(this);

    windowLabel = new QLabel("Window size :", this);
    gridLayout->addWidget(windowLabel, 0, 0, 1, 1);

    windowSpin = new QSpinBox(this);
    windowSpin->setAlignment(Qt::AlignCenter);
    windowSpin->setRange(0, 255);
    windowSpin->setValue(0);
    gridLayout->addWidget(windowSpin, 0, 1, 1, 1);

    deltaLabel = new QLabel("Delta :", this);
    gridLayout->addWidget(deltaLabel, 1, 0, 1, 1);

    deltaSpin = new QSpinBox(this);
    deltaSpin->setAlignment(Qt::AlignCenter);
    deltaSpin->setRange(1, 254);
    deltaSpin->setValue(32);
    gridLayout->addWidget(deltaSpin, 1, 1, 1, 1);

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
    AdaptBimodThreshDialog *dlg = new AdaptBimodThreshDialog(data->window);
    if (dlg->exec()==QDialog::Accepted)
    {
        int window_size = dlg->windowSpin->value();
        int delta = dlg->deltaSpin->value();
        float T = (float)delta / 255.0f;
        thresholdAdaptBimod(data->image, T, window_size);
        emit imageChanged();
    }
}
