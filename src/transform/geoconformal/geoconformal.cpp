#include "geoconform.h"
#include "geoconformal.h"

#define PLUGIN_NAME "GeoConformal"
#define PLUGIN_MENU "Transform/GeoConformal"
#define PLUGIN_VERSION "0.20210414"

// first parameter is name of plugin, usually same as the library file name
Q_EXPORT_PLUGIN2(geoconformal, FilterPlugin);

// ********************** Geo Conformal *********************

void GeoConformal(QImage &img, QString sparams, QString sregion, unsigned iters)
{
    int nk = 0, y, x, yr;
    GCIparams params;
    IMTimage imgin, imgout;
    int imgW = img.width();
    int imgH = img.height();

    params.iters = (iters > 1) ? iters : COUNTG;
    char* chparams = sparams.toLocal8Bit().data();
    params.trans.na = sscanf(chparams, "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", &params.trans.a[0], &params.trans.a[1], &params.trans.a[2], &params.trans.a[3], &params.trans.a[4], &params.trans.a[5], &params.trans.a[6], &params.trans.a[7], &params.trans.a[8], &params.trans.a[9], &params.trans.a[10], &params.trans.a[11], &params.trans.a[12], &params.trans.a[13], &params.trans.a[14], &params.trans.a[15], &params.trans.a[16], &params.trans.a[17], &params.trans.a[18], &params.trans.a[19]);
    if (params.trans.na < 3)
        return;
    char* chregion = sregion.toLocal8Bit().data();
    nk = sscanf(chregion, "%f,%f,%f,%f", &params.rect1.p[0].x, &params.rect1.p[0].y, &params.rect1.p[2].x, &params.rect1.p[2].y);
    if (nk < 4)
        return;

    params.size1.width = imgW;
    params.size1.height = imgH;

// Convert QImage to IMTimage
    imgin = IMTalloc(params.size1, 8 * COUNTC);
    for (y = 0; y < imgin.size.height; y++)
    {
        yr = imgin.size.height - y - 1;
        QRgb *row = (QRgb*)img.constScanLine(y);
        for (x = 0; x < imgin.size.width; x++)
        {
            imgin.p[yr][x].c[0] = qRed(row[x]);
            imgin.p[yr][x].c[1] = qGreen(row[x]);
            imgin.p[yr][x].c[2] = qBlue(row[x]);
            imgin.p[yr][x].c[3] = qAlpha(row[x]);
        }
    }
    params = GCIcalcallparams(params);
    imgout = IMTalloc(params.size2, 8 * COUNTC);
    IMTFilterGeoConform(imgin, imgout, params);
    imgin = IMTfree(imgin);
// Convert IMTimage to QImage
    QImage dstImg(imgout.size.width, imgout.size.height, QImage::Format_ARGB32);
    for (y = 0; y < imgout.size.height; y++)
    {
        yr = imgout.size.height - y - 1;
        QRgb *row = (QRgb*)dstImg.constScanLine(y);
        for (x = 0; x < imgout.size.width; x++)
        {
            row[x] = qRgba( imgout.p[yr][x].c[0], imgout.p[yr][x].c[1], imgout.p[yr][x].c[2], imgout.p[yr][x].c[3]);
        }
    }
    img = dstImg;
}

// **************** Geo Conformal Dialog ******************
GeoConformalDialog:: GeoConformalDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    QGridLayout *gridLayout = new QGridLayout(this);

    QLabel *labelp = new QLabel("Parameters :", this);
    gridLayout->addWidget(labelp, 0, 0, 1, 1);

    textp = new QLineEdit(this);
    gridLayout->addWidget(textp, 0, 1, 1, 1);

    QLabel *labelr = new QLabel("Region :", this);
    gridLayout->addWidget(labelr, 1, 0, 1, 1);

    textr = new QLineEdit(this);
    gridLayout->addWidget(textr, 1, 1, 1, 1);

    QLabel *labeli = new QLabel("Iteration :", this);
    gridLayout->addWidget(labeli, 2, 0, 1, 1);

    texti = new QLineEdit(this);
    gridLayout->addWidget(texti, 2, 1, 1, 1);


    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    gridLayout->addWidget(buttonBox, 3, 0, 1, 2);

    textp->setText( sparams);
    textr->setText( sregion);
    texti->setText( siters);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

QString FilterPlugin:: menuItem()
{
    return QString(PLUGIN_MENU);
}

void FilterPlugin:: onMenuClick()
{
    GeoConformalDialog *dlg = new GeoConformalDialog(data->window);
    if (dlg->exec() == QDialog::Accepted)
    {
        QString sparams = dlg->textp->text();
        QString sregion = dlg->textr->text();
        int iters =  dlg->texti->text().toInt();
        
        GeoConformal(data->image, sparams, sregion, iters);
        emit imageChanged();
    }
}
