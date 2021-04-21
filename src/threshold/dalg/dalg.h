#pragma once
#include "plugin.h"
#include <QDialog>
#include <QSpinBox>

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


class DalgDialog : public QDialog
{
public:
    QSpinBox *countSpin;
    QSpinBox *deltaSpin;

    DalgDialog(QWidget *parent);
};