#pragma once
#include "plugin.h"
#include <QDialog>
#include <QLabel>

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

class HistogramDialog : public QDialog
{
    Q_OBJECT
public:
    uint hist_r[256] = {};
    uint hist_g[256] = {};
    uint hist_b[256] = {};
    uint hist_y[256] = {};
    //widgets
    QLabel *histLabel;

    HistogramDialog(QWidget *parent, QImage &image);
public slots:
    void drawHistogram(bool logarithmic);
};
