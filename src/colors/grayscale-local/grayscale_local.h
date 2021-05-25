#pragma once
#include "plugin.h"
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>

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


class GrayScaleDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *radiusLabel, *samplesLabel, *iterationsLabel;
    QSpinBox *radiusSpin, *samplesSpin, *iterationsSpin;
    QCheckBox *enhanceShadowsBtn;
    QDialogButtonBox *buttonBox;

    GrayScaleDialog(QWidget *parent);
};
