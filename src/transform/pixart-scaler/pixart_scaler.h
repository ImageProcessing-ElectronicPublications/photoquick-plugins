#pragma once
#include <QString>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include "scaler.h"
#include "plugin.h"

class FilterPlugin : public QObject, Plugin
{
    Q_OBJECT
    Q_INTERFACES(Plugin)

public:
    QString menuItem();
    void UpcaleX(int method, int n);

public slots:
    void onMenuClick();

signals:
    void imageChanged();
    void optimumSizeRequested();
    void sendNotification(QString title, QString message);
};

class UpscaleDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *labelMethod;
    QComboBox *comboMethod;
    QStringList itemsMethod = { "ScaleX", "HQX", "xBr"};
    QLabel *labelMult;
    QSpinBox *spinMult;
    QDialogButtonBox *buttonBox;

    UpscaleDialog(QWidget *parent);
};
