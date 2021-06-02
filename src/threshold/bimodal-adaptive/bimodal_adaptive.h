#pragma once
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include "plugin.h"

#define MIN(a,b) ({ __typeof__ (a) _a = (a); \
                    __typeof__ (b) _b = (b); \
                    _a < _b ? _a : _b; })

#define MAX(a,b) ({ __typeof__ (a) _a = (a); \
                    __typeof__ (b) _b = (b); \
                    _a > _b ? _a : _b; })

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


class AdaptBimodThreshDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *windowLabel, *deltaLabel;
    QSpinBox *windowSpin, *deltaSpin;
    QDialogButtonBox *buttonBox;

    AdaptBimodThreshDialog(QWidget *parent);
};
