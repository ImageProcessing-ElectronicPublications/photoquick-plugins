/*  This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed
    Copyright (C) 2021 Arindam Chaudhuri <ksharindam@gmail.com>
*/
#include "photo_frame.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QMouseEvent>// for QDragEnterEvent and QDropEvent
#include <QUrl>
#include <cmath>


#define PLUGIN_NAME "Photo Frame"
#define PLUGIN_MENU "Decorate/Add Photo Frame"
#define PLUGIN_VERSION "1.0"

Q_EXPORT_PLUGIN2(photo-frame, ToolPlugin);

#define MIN(a,b) ((a)<(b) ? (a):(b))

QImage expandBorder(QImage img, int top, int right, int bottom, int left);


FrameDialog:: FrameDialog(QWidget *parent, QImage img, int win_w, int win_h) : QDialog(parent)
{
    setupUi(this);
    resize(win_w, win_h);
    // create canvas
    QHBoxLayout *layout = new QHBoxLayout(scrollAreaWidgetContents);
    layout->setContentsMargins(0, 0, 0, 0);
    canvas = new Canvas(this);
    layout->addWidget(canvas);
    // timer for updating margins
    timer = new QTimer(this);
	timer->setSingleShot(true);
	timer->setInterval(500);
    // connect all signals
    connect(timer, SIGNAL(timeout()), this, SLOT(updateMargins()));
    connect(canvas, SIGNAL(fileDropped(QString)), this, SLOT(setFrame(QString)));
    connect(selectFrameBtn, SIGNAL(clicked()), this, SLOT(openFrame()));
    connect(rotateBtn, SIGNAL(clicked()), this, SLOT(rotateFrame()));
    connect(helpBtn, SIGNAL(clicked()), this, SLOT(showHelp()));
    connect(topMarginSlider, SIGNAL(valueChanged(int)), this, SLOT(onMarginsChange()));
    connect(btmMarginSlider, SIGNAL(valueChanged(int)), this, SLOT(onMarginsChange()));
    connect(leftMarginSlider, SIGNAL(valueChanged(int)), this, SLOT(onMarginsChange()));
    connect(rightMarginSlider, SIGNAL(valueChanged(int)), this, SLOT(onMarginsChange()));

    this->photo = img;
    // scale the canvas to fit to scrollarea width and height
    max_w = win_w - sidePane->width() - 2*9/*margins*/ - 3*6/*spacing*/;
    max_h = win_h - buttonBox->height() - 2*9 - 3*6;

    photo_scaled = photo.scaled(max_w, max_h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    canvas->setPixmap(QPixmap::fromImage(photo_scaled));
}


void
FrameDialog:: openFrame()
{
    QString filefilter = "PNG Images (*.png);;All Files (*)";
    QString filepath = QFileDialog::getOpenFileName(this, "Open Image", "", filefilter);
    setFrame(filepath);
}

void
FrameDialog:: setFrame(QString filename)
{
    if (filename.isEmpty())
        return;
    QImage new_frame(filename);
    if (new_frame.isNull())
        return;
    this->frame = new_frame;
    // rotate frame if frame orientation does not match with photo orientation
    if ((photo.width()>photo.height() && frame.width()<frame.height()) ||
        (photo.width()<photo.height() && frame.width()>frame.height()) )
        rotateFrame();
    else
        drawFrame();
}

void
FrameDialog:: drawFrame()
{
    if (frame.isNull()) {
        canvas->setPixmap(QPixmap::fromImage(photo_scaled));
        return;
    }
    QImage scaled_frame = frame.scaled(photo_scaled.width(), photo_scaled.height());
    QImage img = photo_scaled.copy();
    QPainter painter(&img);
    painter.drawImage(0,0,scaled_frame);
    painter.end();
    canvas->setPixmap(QPixmap::fromImage(img));
}

void
FrameDialog:: rotateFrame()
{
    if (frame.isNull())
        return;
    QTransform tfm;
    tfm.rotate(90);
    frame = frame.transformed(tfm);
    drawFrame();
}

void
FrameDialog:: onMarginsChange()
{
    topLabel->setText(QString("Top : (%1%)").arg(topMarginSlider->value()/6.0));
    btmLabel->setText(QString("Bottom : (%1%)").arg(btmMarginSlider->value()/6.0));
    leftLabel->setText(QString("Left : (%1%)").arg(leftMarginSlider->value()/6.0));
    rightLabel->setText(QString("Right : (%1%)").arg(rightMarginSlider->value()/6.0));
    timer->start();// call updateMargins()
}

void
FrameDialog:: updateMargins()
{
    float fac = MIN(photo.width(), photo.height()) / 600.0;
    QImage img = expandBorder(photo, topMarginSlider->value()*fac, rightMarginSlider->value()*fac,
                        btmMarginSlider->value()*fac, leftMarginSlider->value()*fac);
    photo_scaled = img.scaled(max_w, max_h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    drawFrame();
}

void
FrameDialog:: accept()
{
    float fac = MIN(photo.width(), photo.height()) / 600.0;
    photo = expandBorder(photo, topMarginSlider->value()*fac, rightMarginSlider->value()*fac,
                        btmMarginSlider->value()*fac, leftMarginSlider->value()*fac);
    if (not frame.isNull()) {
        QPainter painter(&photo);
        QImage scaled_frame = frame.scaled(photo.width(), photo.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(0,0,scaled_frame);
        painter.end();
    }
    QDialog::accept();
}

void
FrameDialog:: showHelp()
{
    QString helptext =
        "Decorate Photo by adding Frame :\n\n"
        "1. Download some photo frames as png file from internet.\n\n"
        "2. Click on 'Select Frame ...' button to select preferred frame. Or you can"
            " drag and drop a frame from file manager.\n\n"
        "3. Move margins sliders if desired areas are not visible (covered by frame).\n\n"
        "4. Click OK.";
    QMessageBox::about(this, "Photo Frame", helptext);
}


// Canvas for displaying photo and frame
Canvas:: Canvas(QWidget *parent) : QLabel(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMouseTracking(true);
    setAcceptDrops(true);
}

void
Canvas:: dragEnterEvent(QDragEnterEvent *ev)
{
    if (ev->mimeData()->hasUrls())
        ev->acceptProposedAction();
    else
        ev->ignore();
}

void
Canvas:: dropEvent(QDropEvent *ev)
{
    if ( ev->mimeData()->hasUrls() )
    {
        QUrl url = ev->mimeData()->urls().first();
        QString file_name = url.toLocalFile();
        if (!file_name.isEmpty() && file_name.endsWith(".png", Qt::CaseInsensitive)) {
            emit fileDropped(file_name);
        }
    }
    ev->ignore();
}

// expand borders of an image by given pixels
QImage expandBorder(QImage img, int top, int right, int bottom, int left)
{
    if (top==0 && right==0 && bottom==0 && left==0)
        return img;
    int w = img.width();
    int h = img.height();
    QImage dst = QImage(left+w+right, top+h+bottom, img.format());
    // copy all image pixels at the center
    QRgb *row, *dstRow;
    for (int y=0; y<h; y++) {
        row = (QRgb*)img.constScanLine(y);
        dstRow = (QRgb*)dst.scanLine(top+y);
        dstRow += left;
        memcpy(dstRow, row, w*4);
    }
    // duplicate first row as top margin
    row = (QRgb*)img.constScanLine(0);
    for (int i=0; i<top; i++) {
        dstRow = (QRgb*)dst.scanLine(i);
        dstRow += left;
        memcpy(dstRow, row, w*4);
    }
    // duplicate last row
    row = (QRgb*)img.constScanLine(h-1);
    for (int i=0; i<bottom; i++) {
        dstRow = (QRgb*)dst.scanLine(top+h+i);
        dstRow += left;
        memcpy(dstRow, row, w*4);
    }
    // duplicate left and right sides
    QRgb left_clr, right_clr;
    for (int y=0; y<h; y++) {
        dstRow = (QRgb*)dst.scanLine(top+y);
        left_clr = ((QRgb*)img.constScanLine(y))[0];
        for (int x=0; x<left; x++) {
            dstRow[x] = left_clr;
        }
        right_clr = ((QRgb*)img.constScanLine(y))[w-1];
        for (int x=0; x<right; x++) {
            dstRow[left+w+x] = right_clr;
        }
    }
    // duplicate corner pixels
    row = (QRgb*)img.constScanLine(0);
    for (int y=0; y<top; y++) {
        dstRow = (QRgb*)dst.scanLine(y);
        for (int x=0; x<left; x++) {
            dstRow[x] = row[0];
        }
        for (int x=0; x<right; x++) {
            dstRow[left+w+x] = row[w-1];
        }
    }
    row = (QRgb*)img.constScanLine(h-1);
    for (int y=0; y<bottom; y++) {
        dstRow = (QRgb*)dst.scanLine(top+h+y);
        for (int x=0; x<left; x++) {
            dstRow[x] = row[0];
        }
        for (int x=0; x<right; x++) {
            dstRow[left+w+x] = row[w-1];
        }
    }
    return dst;
}

// ********* ----------- Plugin ---------- ********* //

QString ToolPlugin:: menuItem()
{
    return QString(PLUGIN_MENU);
}

void ToolPlugin:: onMenuClick()
{
    FrameDialog *dlg = new FrameDialog(data->window, data->image, 1020,data->max_window_h);
    if (dlg->exec()==QDialog::Accepted){
        data->image = dlg->photo;
        emit imageChanged();
    }
}
