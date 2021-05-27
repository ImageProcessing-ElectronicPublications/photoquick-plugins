#pragma once
#include <QDebug>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include "plugin.h"

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
    QGridLayout *gridLayout;
    QLabel *contrastLabel, *saturationLabel;
    QDoubleSpinBox *contrastSpin, *saturationSpin;
    QDialogButtonBox *buttonBox;

    Mantiuk06Dialog(QWidget *parent);
};
