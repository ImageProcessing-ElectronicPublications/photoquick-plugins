#include "deskew.h"

#define PLUGIN_NAME "Deskew"
#define PLUGIN_MENU "Transform/Deskew"
#define PLUGIN_VERSION "4.4.2"

// first parameter is name of plugin, usually same as the library file name
Q_EXPORT_PLUGIN2(deskew, FilterPlugin);

// ----------------------------------------------------------

unsigned int PageTools_Next_Pow2(unsigned int n)
{
    unsigned int retval = 1;
    while(retval < n)
        retval <<= 1;
    return retval;
}

void PageTools_Radon(QImage &p_im, int thres, int sign, unsigned int sharpness[])
{
    unsigned int h, w, w2, s, step, m;
    unsigned int ir, ic, ics, i, j, acc, diff;
    unsigned char val;
    unsigned short int *p1, *p2, *x1, *x2;
    unsigned short int *s1, *s2, *t1, *t2, *aux, *col;
    QRgb pix;

    h = p_im.height();
    w = p_im.width();;
    w2 = PageTools_Next_Pow2(w);
    s = h * w2;
    p1 = (unsigned short int*)malloc(sizeof(unsigned short int) * s);
    p2 = (unsigned short int*)malloc(sizeof(unsigned short int) * s);
    // Fill in the first table
    memset(p1, 0, sizeof(unsigned short int) * s);

    for(ir = 0; ir < h; ir++)
    {
        QRgb *row = (QRgb*)p_im.constScanLine(ir);
        for(ic = 0; ic < w; ic++)
        {
            ics = ic;
            if(sign > 0)
                ics = w - 1 - ic;
            pix = row[ics];
            val = (((qRed(pix) + qGreen(pix) + qBlue(pix)) > (thres + thres + thres)) ? 1 : 0);

            p1[h * ic + ir] = val;
        }
    }

    // Iterate
    x1 = p1;
    x2 = p2;
    step = 1;
    while(step < w2)
    {
        for(i = 0; i < w2; i += 2 * step)
        {
            for(j = 0; j < step; j++)
            {
                // Columns-sources:
                s1 = x1 + h * (i + j);
                s2 = x1 + h * (i + j + step);

                // Columns-targets:
                t1 = x2 + h * (i + 2 * j);
                t2 = x2 + h * (i + 2 * j + 1);
                for(m = 0; m < h; m++)
                {
                    t1[m] = s1[m];
                    t2[m] = s1[m];
                    if(m + j < h)
                        t1[m] += s2[m + j];
                    if(m + j + 1 < h)
                        t2[m] += s2[m + j + 1];
                }
            }
        }
        // Swap the tables:
        aux = x1;
        x1 = x2;
        x2 = aux;
        // Increase the step:
        step += step;
    }
    // Now, compute the sum of squared finite differences:
    for(ic = 0; ic < w2; ic++)
    {
        acc = 0;
        col = x1 + h * ic;
        for(ir = 0; ir + 1 < h; ir++)
        {
            diff = (int)(col[ir])-(int)(col[ir + 1]);
            acc += diff * diff;
        }
        sharpness[w2 - 1 + sign * ic] = acc;

    }
    free(p1);
    free(p2);
}

// ----------------------------------------------------------

float PageTools_FindSkew(QImage &p_im, int thres)
{
    unsigned int h, w, w2, ssize, s;
    unsigned int i, imax = 0, vmax=0;
    int iskew;
    float sum = 0.0f, ret;
    unsigned int *sharpness;

    h = p_im.height();
    w = p_im.width();;
    w2 = PageTools_Next_Pow2(w);
    ssize = 2 * w2 - 1; // Size of sharpness table
    sharpness = (unsigned int*)malloc(sizeof(unsigned int) * ssize);
    PageTools_Radon(p_im, thres, 1, sharpness);
    PageTools_Radon(p_im, thres, -1, sharpness);
    for(i = 0; i < ssize; i++)
    {
        s = sharpness[i];

        if(s > vmax)
        {
            imax = i;
            vmax = s;
        }
        sum += s;
    }

    if (vmax <= 3 * sum / h)
    {
        ret = 0.0f; // Heuristics !!!
    }
    else
    {
        iskew = imax - w2 + 1;
        ret = atan((float)iskew / w2);
    }
    free(sharpness);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
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
QRgb InterpolateBiCubic (QImage &img, float y, float x)
{
    int height, width, i, d, dn, xi, yi, xf, yf;
    float d0, d2, d3, a0, a1, a2, a3;
    float dx, dy, k2 = 1.0f / 2.0f, k3 = 1.0f / 3.0f, k6 = 1.0f / 6.0f;
    float Cc, C[4];
    int Ci, pt[4];
    QRgb imgpix;

    height = img.height();
    width = img.width();;
    yi = (int)y;
    yi = (yi < 0) ? 0 : (yi < height) ? yi : (height - 1);
    xi = (int)x;
    xi = (xi < 0) ? 0 : (xi < width) ? xi : (width - 1);
    dy = y - yi;
    dx = x - xi;
    dn = (img.hasAlphaChannel()) ? 4 : 3;
    for(d = 0; d < dn; d++)
    {
        for(i = -1; i < 3; i++)
        {
            yf = (int)y + i;
            yf = (yf < 0) ? 0 : (yf < height) ? yf : (height - 1);
            QRgb *row = (QRgb*)img.constScanLine(yf);
            xf = (int)x;
            xf = (xf < 0) ? 0 : (xf < width) ? xf : (width - 1);
            a0 = SelectChannelPixel(row[xf], d);
            xf = (int)x - 1;
            xf = (xf < 0) ? 0 : (xf < width) ? xf : (width - 1);
            d0 = SelectChannelPixel(row[xf], d);
            d0 -= a0;
            xf = (int)x + 1;
            xf = (xf < 0) ? 0 : (xf < width) ? xf : (width - 1);
            d2 = SelectChannelPixel(row[xf], d);
            d2 -= a0;
            xf = (int)x + 2;
            xf = (xf < 0) ? 0 : (xf < width) ? xf : (width - 1);
            d3 = SelectChannelPixel(row[xf], d);
            d3 -= a0;
            a1 = -k3 * d0 + d2 - k6 * d3;
            a2 = k2 * d0 + k2 * d2;
            a3 = -k6 * d0 - k2 * d2 + k6 * d3;
            C[i + 1] = a0 + (a1 + (a2 + a3 * dx) * dx) * dx;
        }
        a0 = C[1];
        d0 = C[0] - a0;
        d2 = C[2] - a0;
        d3 = C[3] - a0;
        a1 = -k3 * d0 + d2 - k6 * d3;
        a2 = k2 * d0 + k2 * d2;
        a3 = -k6 * d0 - k2 * d2 + k6 * d3;
        Cc = a0 + (a1 + (a2 + a3 * dy) * dy) * dy;
        Ci = (int)(Cc + 0.5f);
        pt[d] = (Ci < 0) ? 0 : (Ci > 255) ? 255 : Ci;
    }
    if (dn > 3)
        imgpix = qRgba(pt[0], pt[1], pt[2],  pt[3]);
    else
        imgpix = qRgb(pt[0], pt[1], pt[2]);

    return imgpix;
}

QImage FilterRotate (QImage p_im, float angle)
{
    unsigned int height, width, y, x;
    float yt, xt, yr, xr, ktc, kts;
    height = p_im.height();
    width = p_im.width();
    QImage d_im = p_im.copy();

    kts = sin(angle);
    ktc = cos(angle);
    for (y = 0; y < height; y++ )
    {
        yt = (float)y;
        yt -= (float)height * 0.5f;
        QRgb *row = (QRgb*)d_im.constScanLine(y);
        for (x = 0; x < width; x++ )
        {
            xt = (float)x;
            xt -= (float)width * 0.5f;
            yr = ktc * yt - kts * xt;
            yr += (float)height * 0.5f;
            xr = ktc * xt + kts * yt;
            xr += (float)width * 0.5f;
            if (yr >= 0.0f && yr < height && xr >= 0.0f && xr < width)
            {
                row[x] = InterpolateBiCubic (p_im, yr, xr);
            }
        }
    }
    return d_im;
}

/*
void Deskew(QImage &img, int thres)
{
    float angle;
    QImage rotated;

    angle = PageTools_FindSkew(img, thres);
    rotated = FilterRotate(img, angle);
    img = rotated.copy();
}
*/

// **************** Deskew Dialog ******************
DeskewDialog:: DeskewDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(260, 98);

    gridLayout = new QGridLayout(this);

    thresLabel = new QLabel("Threshold :", this);
    gridLayout->addWidget(thresLabel, 0, 0, 1, 1);

    thresSpin = new QSpinBox(this);
    thresSpin->setAlignment(Qt::AlignCenter);
    thresSpin->setRange(0,255);
    thresSpin->setValue(127);
    gridLayout->addWidget(thresSpin, 0, 1, 1, 1);

    buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    gridLayout->addWidget(buttonBox, 1, 0, 1, 2);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

DeskewDialogWarning:: DeskewDialogWarning(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(260, 98);

    gridLayout = new QGridLayout(this);

    angleLabel = new QLabel("Angle (radians):", this);
    gridLayout->addWidget(angleLabel, 0, 0, 1, 1);

    angleText = new QLineEdit("", this);
    gridLayout->addWidget(angleText, 0, 1, 1, 1);

    buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    gridLayout->addWidget(buttonBox, 1, 0, 1, 2);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

QString FilterPlugin:: menuItem()
{
    return QString(PLUGIN_MENU);
}

void FilterPlugin:: onMenuClick()
{
    float angle;
    QImage rotated;

    DeskewDialog *dlg = new DeskewDialog(data->window);
    if (dlg->exec()==QDialog::Accepted)
    {
        int thres = dlg->thresSpin->value();
//        Deskew(data->image, thres);

        angle = PageTools_FindSkew(data->image, thres);
        DeskewDialogWarning *dlgw = new DeskewDialogWarning(data->window);
        dlgw->angleText->setText(QString("%1").arg(angle));
        if (dlgw->exec()==QDialog::Accepted)
        {
            angle =  dlgw->angleText->text().toFloat();
            rotated = FilterRotate(data->image, angle);
            data->image = rotated.copy();
        }
        emit imageChanged();
    }
}
