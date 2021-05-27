#pragma once
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
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


class DitherDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *labelcount, *labeldelta, *labelmult;
    QSpinBox *countSpin, *deltaSpin, *kpgSpin;
    QDialogButtonBox *buttonBox;

    DitherDialog(QWidget *parent);
};
