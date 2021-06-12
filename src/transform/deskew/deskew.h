#pragma once
#include <cmath>
#include <QDialog>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include "plugin.h"

class FilterPlugin : public QObject, Plugin
{
    Q_OBJECT
    Q_INTERFACES(Plugin)
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID Plugin_iid)
#endif

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
