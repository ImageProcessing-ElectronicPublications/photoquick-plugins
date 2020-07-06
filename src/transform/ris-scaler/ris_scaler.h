#pragma once
#include "plugin.h"

class FilterPlugin : public QObject, Plugin
{
    Q_OBJECT
    Q_INTERFACES(Plugin)

public:
    QStringList menuItems();
    void filterHRISX(int n);
    void filterGSampleX(int n);
    void filterMeanX(int n);

public slots:
    void handleAction(QAction *action, int type);
    void filterHRIS2x();
    void filterHRIS3x();
    void filterGSample2x();
    void filterGSample3x();
    void filterMean2r();
    void filterMean3r();

signals:
    void imageChanged();
    void optimumSizeRequested();
    void sendNotification(QString title, QString message);
};
