#pragma once
#include <cmath>
#include <QDialog>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include "plugin.h"

class ToolPlugin : public QObject, Plugin
{
    Q_OBJECT
    Q_INTERFACES(Plugin)
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID Plugin_iid)
#endif

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
    //widgets
    QGridLayout *gridLayout;
    QWidget *container;
    QDialogButtonBox *buttonBox;
    QHBoxLayout *hLayout;
    QCheckBox *logBtn;
    QLabel *histLabel;

    HistogramDialog(QWidget *parent, QImage &image);
public slots:
    void drawHistogram(bool logarithmic);
};
