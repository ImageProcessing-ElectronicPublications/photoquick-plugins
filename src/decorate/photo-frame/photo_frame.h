/*  This file is a part of PhotoQuick Plugins project, and is GNU GPLv3 licensed
    Copyright (C) 2021 Arindam Chaudhuri <ksharindam@gmail.com>
*/
#include "plugin.h"
#include "ui_frame_dialog.h"
#include <QTimer>

class ToolPlugin : public QObject, Plugin
{
    Q_OBJECT
    Q_INTERFACES(Plugin)

public:
    QString menuItem();

public slots:
    void onMenuClick();

signals:
    void imageChanged();
    void optimumSizeRequested();
    void sendNotification(QString title, QString message);
};


class Canvas : public QLabel
{
    Q_OBJECT
public:
    Canvas(QWidget *parent);
    void dragEnterEvent(QDragEnterEvent *ev);
    void dropEvent(QDropEvent *ev);
signals:
    void fileDropped(QString);
};


class FrameDialog : public QDialog, public Ui_FrameDialog
{
    Q_OBJECT
public:
    FrameDialog(QWidget *parent, QImage img, int win_w, int win_h);
    void accept();
    void drawFrame();
    // Variables
    QImage photo;
    QImage frame;
    QImage photo_scaled;
    int max_w, max_h;
    float scale = 1.0;
    QTimer *timer;
    Canvas *canvas;
public slots:
    void openFrame();
    void setFrame(QString);
    void rotateFrame();
    void onMarginsChange();
    void updateMargins();
    void showHelp();
};
