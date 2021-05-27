#pragma once
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
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


class BimodThreshDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *countLabel, *deltaLabel;
    QSpinBox *countSpin, *deltaSpin;
    QCheckBox *medianBtn;
    QDialogButtonBox *buttonBox;

    BimodThreshDialog(QWidget *parent);
};
