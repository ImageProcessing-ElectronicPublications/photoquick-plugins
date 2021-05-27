#pragma once
#include <QString>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include "plugin.h"
#include "libris.h"

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
    QLabel *labelMethod, *labelMult;
    QComboBox *comboMethod;
    QStringList itemsMethod = { "GSample", "HRIS", "Mean"};
    QSpinBox *spinMult;
    QDialogButtonBox *buttonBox;

    RISDialog(QWidget *parent);
};
