#pragma once
#include "plugin.h"
#include <cmath>
#include <QDialog>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>

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

class DeskewDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *thresLabel;
    QSpinBox *thresSpin;
    QDialogButtonBox *buttonBox;

    DeskewDialog(QWidget *parent);
};

class DeskewDialogWarning : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *angleLabel;
    QLineEdit *angleText;
    QDialogButtonBox *buttonBox;

    DeskewDialogWarning(QWidget *parent);
};
