#pragma once
#include "plugin.h"
#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>

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
    QSpinBox *redSpin;
    QSpinBox *greenSpin;
    QSpinBox *blueSpin;

    QuantDialog(QWidget *parent);
};
