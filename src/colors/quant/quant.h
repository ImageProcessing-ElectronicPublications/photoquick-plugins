#pragma once
#include "plugin.h"
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
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


class QuantDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *redLabel, *greenLabel, *blueLabel;
    QSpinBox *redSpin, *greenSpin, *blueSpin;
    QDialogButtonBox *buttonBox;

    QuantDialog(QWidget *parent);
};
