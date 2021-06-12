#include "geoconform.h"
#include "geoconformal.h"

#define PLUGIN_NAME "GeoConformal"
#define PLUGIN_MENU "Transform/Geometry/GeoConformal"
#define PLUGIN_VERSION "0.20210416"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Q_EXPORT_PLUGIN2(geoconformal, FilterPlugin);
#endif

// ********************** Geo Conformal *********************

GCIparams GeoConformalParams(QImage &img, QString sparams, QString sregion, int iters, int margin)
{
    GCIparams params;
    int imgW = img.width();
    int imgH = img.height();

    params.complete = 0;
    params.iters = (iters > 1) ? iters : COUNTG;
    params.margin = (margin > 0) ? margin : COUNTM;
    char* chparams = sparams.toLocal8Bit().data();
    params.trans.na = sscanf(chparams, "%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf;%lf", &params.trans.a[0], &params.trans.a[1], &params.trans.a[2], &params.trans.a[3], &params.trans.a[4], &params.trans.a[5], &params.trans.a[6], &params.trans.a[7], &params.trans.a[8], &params.trans.a[9], &params.trans.a[10], &params.trans.a[11], &params.trans.a[12], &params.trans.a[13], &params.trans.a[14], &params.trans.a[15], &params.trans.a[16], &params.trans.a[17], &params.trans.a[18], &params.trans.a[19]);
    char* chregion = sregion.toLocal8Bit().data();
    params.rect1.n = sscanf(chregion, "%f;%f;%f;%f", &params.rect1.p[0].x, &params.rect1.p[0].y, &params.rect1.p[2].x, &params.rect1.p[2].y);

    params.size1.width = imgW;
    params.size1.height = imgH;

    if (params.trans.na > 2 && params.rect1.n > 3)
        params = GCIcalcallparams(params);

    return params;
}

void GeoConformal(QImage &img, GCIparams params)
{
    unsigned y, x, yr;
    IMTimage imgin, imgout;
    if ((params.trans.na < 3) || (params.rect1.n < 4))
        return;

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
    img = dstImg.copy();
}

// **************** Geo Conformal Dialog ******************
GeoConformalDialog:: GeoConformalDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(320, 158);

    gridLayout = new QGridLayout(this);

    labelp = new QLabel("Parameters :", this);
    gridLayout->addWidget(labelp, 0, 0, 1, 1);

    textp = new QLineEdit(this);
    gridLayout->addWidget(textp, 0, 1, 1, 1);

    labelr = new QLabel("Region :", this);
    gridLayout->addWidget(labelr, 1, 0, 1, 1);

    textr = new QLineEdit(this);
    gridLayout->addWidget(textr, 1, 1, 1, 1);

    labeli = new QLabel("Iteration :", this);
    gridLayout->addWidget(labeli, 2, 0, 1, 1);

    texti = new QLineEdit(this);
    gridLayout->addWidget(texti, 2, 1, 1, 1);

    labelm = new QLabel("Margin :", this);
    gridLayout->addWidget(labelm, 3, 0, 1, 1);

    textm = new QLineEdit(this);
    gridLayout->addWidget(textm, 3, 1, 1, 1);

    buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    gridLayout->addWidget(buttonBox, 4, 0, 1, 2);

    textp->setText( sparams);
    textr->setText( sregion);
    texti->setText( siters);
    textm->setText( smargin);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

// **************** Geo Conformal Warning Dialog ******************
GeoConformalWarningDialog:: GeoConformalWarningDialog(QWidget *parent) : QDialog(parent)
{
    this->setWindowTitle(PLUGIN_NAME);
    this->resize(480, 88);

    gridLayout = new QGridLayout(this);

    labelnc = new QLabel("New Region = ", this);
    gridLayout->addWidget(labelnc, 0, 0, 1, 1);

    textnc = new QLineEdit("", this);
    gridLayout->addWidget(textnc, 0, 1, 1, 1);

    labelns = new QLabel("New Size = ", this);
    gridLayout->addWidget(labelns, 1, 0, 1, 1);

    textns = new QLineEdit("", this);
    gridLayout->addWidget(textns, 1, 1, 1, 1);

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
    GeoConformalDialog *dlg = new GeoConformalDialog(data->window);
    if (dlg->exec() == QDialog::Accepted)
    {
        QString sparams = dlg->textp->text();
        QString sregion = dlg->textr->text();
        int iters =  dlg->texti->text().toInt();
        int margin =  dlg->textm->text().toInt();

        GCIparams params = GeoConformalParams(data->image, sparams, sregion, iters, margin);
        const char ff = 'f';

        GeoConformalWarningDialog *dlgw = new GeoConformalWarningDialog(data->window);
        if (params.complete)
        {
            dlgw->textnc->setText(QString("(%1,%2)-(%3,%4)").arg(QString::number(params.rect2.min.x, ff)).arg(QString::number(params.rect2.min.y, ff)).arg(QString::number(params.rect2.max.x, ff)).arg(QString::number(params.rect2.max.y, ff)));
            dlgw->textns->setText(QString("%1 x %2").arg(params.size2.width).arg(params.size2.height));
        }
        else
        {
            dlgw->labelnc->setText(QString("ERROR: "));
            dlgw->textnc->setText(QString("Bad parameters!"));
            dlgw->labelns->setText(QString("Count: "));
            dlgw->textns->setText(QString("P = %1 (> 2), R = %2 (= 4)").arg(params.trans.na).arg(params.rect1.n));
        }

        if ((dlgw->exec() == QDialog::Accepted) && (params.complete))
        {
            GeoConformal(data->image, params);
            emit imageChanged();
        }

    }
}
