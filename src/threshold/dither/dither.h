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


class DitherDialog : public QDialog
{
public:
    QSpinBox *countSpin;
    QSpinBox *deltaSpin;
    QSpinBox *kpgSpin;

    DitherDialog(QWidget *parent);
};
