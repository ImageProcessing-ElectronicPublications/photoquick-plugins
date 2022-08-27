#include "xpng.h"

#define PLUGIN_NAME "XPNG"
#define PLUGIN_MENU "Filters/Threshold/XPNG"
#define PLUGIN_VERSION "1.5"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(quant, FilterPlugin);
#endif

// **************** XPNG Dialog ******************
XPNGDialog:: XPNGDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    gridLayout = new QGridLayout(this);

    LevelLabel = new QLabel("Level :", this);
    gridLayout->addWidget(LevelLabel, 0, 0, 1, 1);
    LevelSpin = new QSpinBox(this);
    LevelSpin->setAlignment(Qt::AlignCenter);
    LevelSpin->setRange(0, 255);
    LevelSpin->setValue(32);
    gridLayout->addWidget(LevelSpin, 0, 1, 1, 1);

    RadiusLabel = new QLabel("Radius :", this);
    gridLayout->addWidget(RadiusLabel, 1, 0, 1, 1);
    RadiusSpin = new QSpinBox(this);
    RadiusSpin->setAlignment(Qt::AlignCenter);
    RadiusSpin->setRange(1, 16);
    RadiusSpin->setValue(2);
    gridLayout->addWidget(RadiusSpin, 1, 1, 1, 1);

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
    int y, x, r, g, b, a, dn;
    uint8_t clevel;
    int32_t h, w, radius;
    size_t pngsize, k;
    png_bytep buffer;
    QRgb *row;
    XPNGDialog *dlg = new XPNGDialog(data->window);
    if (dlg->exec()==QDialog::Accepted)
    {
        clevel = dlg->LevelSpin->value();
        radius = dlg->RadiusSpin->value();
        w = data->image.width();
        h = data->image.height();
        dn = (data->image.hasAlphaChannel()) ? 4 : 3;
        pngsize = w * h * XPNG_BPP * sizeof(png_byte);
        buffer = (png_bytep)malloc(pngsize);
        if (buffer == NULL)
            return;
        k = 0;
        for (y = 0; y < h; y++)
        {
            row = (QRgb*)data->image.constScanLine(y);
            for (x = 0; x < w; x++)
            {
                buffer[k] = qRed(row[x]);
                k++;
                buffer[k] = qGreen(row[x]);
                k++;
                buffer[k] = qBlue(row[x]);
                k++;
                buffer[k] = qAlpha(row[x]);
                k++;
            }
        }
        xpng(buffer, w, h, pngsize, clevel, radius);
        k = 0;
        for (y = 0; y < h; y++)
        {
            row = (QRgb*)data->image.constScanLine(y);
            for (x = 0; x < w; x++)
            {
                r = buffer[k];
                k++;
                g = buffer[k];
                k++;
                b = buffer[k];
                k++;
                a = buffer[k];
                k++;
                row[x] = (dn > 3) ? qRgba(r, g, b, a) : qRgb(r, g, b);
            }
        }
        free(buffer);
        emit imageChanged();
    }
}

#ifdef __cplusplus
extern "C" {
#endif

int16_t byteclamp16_t(int16_t c)
{
    int16_t buff[3] = {0, c, 255};
    return buff[ (int)(c > 0) + (int)(c > 255) ];
}

int32_t byteclamp32_t(int32_t c, int32_t a, int32_t b)
{
    int32_t buff[3] = {a, c, b};
    return buff[ (int)(c > a) + (int)(c > b) ];
}

/* Returns pointer to a certain address in a 24 bit datafield */
png_bytep pix(int32_t i, int32_t j, uint8_t c, int32_t w, png_bytep data)
{
    return &(data[(i*w+j)*XPNG_BPP+c]);
}

/* Returns the difference between two bytes for error calculation */
uint8_t pix_err(png_byte x1, png_byte x2)
{
    uint8_t a1 = x1, a2 = x2;
    return (a1 > a2) ? (a1 - a2) : (a2 - a1);
}

/* Average predictor for PNG */
png_byte pix_avg(int32_t i, int32_t j, uint8_t c, int32_t w, png_bytep data)
{
    int16_t p;
    int32_t ip, jp;
    png_byte left, up;
    ip = (i > 0) ? (i - 1) : 0;
    jp = (j > 0) ? (j - 1) : 0;
    left = *pix(i,jp,c,w,data);
    up = *pix(ip,j,c,w,data);

    p = up;
    p += left;
    p /= 2;
    return p;
}

/* Blured images: Ib = M(I, w, w); w - window (+-) radius */
png_byte pix_blur(int32_t i, int32_t j, uint8_t c, int32_t h, int32_t w, int32_t radius, png_bytep data)
{
    int16_t M = 0;
    int32_t ii, jj, ir, jr, N = 0;
    int64_t p0, p, X = 0;

    p0 = *pix(i, j, c, w, data);
    for (ii = i - radius; ii <= i + radius; ii++)
    {
        ir = byteclamp32_t(ii, 0, h-1);
        for (jj = j - radius; jj <= j + radius; jj++)
        {
            jr = byteclamp32_t(jj, 0, w-1);
            p = *pix(ir, jr, c, w, data);
            X += p;
            N++;
        }
    }
    X -= p0;
    N--;
    M = (N > 0) ? (X / N) : p0;
    M = byteclamp16_t(M);
    return M;
}

/* Calculates noisiness - used for adaptive quantization
 * i = I - Ib; Ib - blur area
 * e = Sum(i*i) / n - M(i)*M(i);
 *  */
png_byte pix_noise(int32_t i, int32_t j, uint8_t c, int32_t h, int32_t w, int32_t radius, png_bytep data, png_bytep blur)
{
    int16_t err = 0;
    int32_t ii, jj, ir, jr, N = 0;
    int64_t p, b, X = 0, X2 = 0;

    for (ii = i - radius; ii <= i + radius; ii++)
    {
        ir = byteclamp32_t(ii, 0, h-1);
        for (jj = j - radius; jj <= j + radius; jj++)
        {
            jr = byteclamp32_t(jj, 0, w-1);
            p = *pix(ir, jr, c, w, data);
            b = *pix(ir, jr, c, w, blur);
            p -= b;
            X += p;
            X2 += p * p;
            N++;
        }
    }
    if (N > 0)
    {
        X /= N;
        X2 /= N;
        err = sqrt(X2 - X * X);
        err = byteclamp16_t(err);
    }
    return err;
}

void xpng(png_bytep buffer, int32_t w, int32_t h, size_t pngsize, uint8_t clevel, int32_t radius)
{
    uint8_t c, d, delta;
    uint16_t jumpsize, qlevel;
    int32_t i, j, refdist;
    uint64_t f, freq[256] = {};
    size_t rd;
    png_byte base, target, ltarget, rtarget, approx, a;

    png_bytep diff; /* Residuals from predictor */
    png_bytep noise; /* Local noisiness */

    qlevel = clevel;
    qlevel = sqrt(qlevel * 256 * 256);

    diff = (png_bytep)malloc(pngsize);
    noise = (png_bytep)malloc(pngsize);

    if (buffer == NULL || diff == NULL || noise == NULL)
    {
        fprintf(stderr, "xpng: error: insufficient memory\n");
        return;
    }

    /* Calculate local noisiness */
    rd = 0;
    for (i = 0; i < h; i++)
        for (j = 0; j < w; j++)
            for (c = 0; c < XPNG_BPP; c++)
            {
                diff[rd] = pix_blur(i,j,c,h,w,radius,buffer);
                rd++;
            }
    rd = 0;
    for (i = 0; i < h; i++)
        for (j = 0; j < w; j++)
            for (c = 0; c < XPNG_BPP; c++)
            {
                noise[rd] = pix_noise(i,j,c,h,w,radius,buffer,diff);
                rd++;
            }

    /* Specify the preference levels of each quantization */
    for (jumpsize = 1; jumpsize <= 128; jumpsize *= 2)
        for (i = -128; i <= 128; i += jumpsize)
            freq[(uint8_t)i] = jumpsize;

    /* Go through image and adaptively quantize for noise masking */
    for (i = 0; i < h; i++)
    {
        refdist = 1;
        for (j = 0; j < w; j++)
        {
            for (c = 0; c < XPNG_BPP; c++)
            {
                /* Left predictor for first row, avg. for others */
                target = *pix(i,j,c,w,buffer);
                base = pix_avg(i,j,c,w,buffer);

                /* Calculate tolerance using comp. level and local noise */
                delta = (qlevel + (uint32_t) *pix(i,j,c,w,noise) * 256 * qlevel / (2047 + qlevel)) / 256;

                /* Calculate interval based on tolerance */
                ltarget = (target > delta) ? (target - delta) : 0;
                rtarget = ((255 - target) > delta) ? (target + delta) : 255;

                /* Find best quantization within allowable tolerance */
                approx = target;
                f = 0;
                a = ltarget;
                do
                {
                    d = a-base;
                    if (freq[d] > f)
                    {
                        approx = a;
                        f = freq[d];
                    }
                }
                while (a++ != rtarget);

                *pix(i,j,c,w,diff) = approx - base;
                /*freq[*pix(i,j,c,diff)]++;*/
                *pix(i,j,c,w,buffer) = approx;

                /* Use recent value for runlength encoding if possible */
                if (pix_err(target,base+*(pix(i,j,c,w,diff)-refdist)) < delta)
                {
                    *pix(i,j,c,w,diff) = *(pix(i,j,c,w,diff)-refdist);
                    *pix(i,j,c,w,buffer) = base + *pix(i,j,c,w,diff);
                    continue;
                }
                for (rd = 1; rd <= (j * XPNG_BPP + c) && rd <= 4; rd++)
                {
                    if (pix_err(target,base+*(pix(i,j,c,w,diff)-rd)) < delta)
                    {
                        refdist = rd;
                        *pix(i,j,c,w,diff) = *(pix(i,j,c,w,diff)-rd);
                        *pix(i,j,c,w,buffer) = base + *pix(i,j,c,w,diff);
                        break;
                    }
                }
            }
        }
    }
    free(diff);
    free(noise);
}

#ifdef __cplusplus
}
#endif
