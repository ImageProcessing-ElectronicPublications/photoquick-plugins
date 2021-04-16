#pragma once
#include "plugin.h"
#include <QString>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
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


class GeoConformalDialog : public QDialog
{
public:
    QLineEdit *textp;
    QLineEdit *textr;
    QLineEdit *texti;
    QString sparams = "0,0,1,0";
    QString sregion = "0,0,100,100";
    QString siters = "10";

    GeoConformalDialog(QWidget *parent);
};

class GeoConformalWarningDialog : public QDialog
{
public:
    QLabel *labelnc;
    QLineEdit *textnc;
    QLabel *labelns;
    QLineEdit *textns;
    QString snc;
    QString sns;

    GeoConformalWarningDialog(QWidget *parent);
};
