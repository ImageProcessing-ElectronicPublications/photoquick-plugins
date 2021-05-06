#pragma once
#include "plugin.h"
#include <QString>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>

class FilterPlugin : public QObject, Plugin
{
    Q_OBJECT
    Q_INTERFACES(Plugin)

public:
    QString menuItem();
    void filterHRISX(int n);
    void filterGSampleX(int n);
    void filterMeanX(int n);

public slots:
    void onMenuClick();

signals:
    void imageChanged();
    void optimumSizeRequested();
    void sendNotification(QString title, QString message);
};

class RISDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *labelMethod;
    QComboBox *comboMethod;
    QStringList itemsMethod = { "GSample", "HRIS", "Mean"};
    QLabel *labelMult;
    QSpinBox *spinMult;
    QDialogButtonBox *buttonBox;

    RISDialog(QWidget *parent);
};
