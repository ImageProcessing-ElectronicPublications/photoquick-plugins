#pragma once
#include "plugin.h"
#include <QDialog>
#include <QDoubleSpinBox>


class FilterPlugin : public QObject, Plugin
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


class Mantiuk06Dialog : public QDialog
{
public:
    QDoubleSpinBox *contrastSpin;
    QDoubleSpinBox *saturationSpin;

    Mantiuk06Dialog(QWidget *parent);
};
