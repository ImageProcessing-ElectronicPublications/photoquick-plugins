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


class DalgDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *labelcount, *labeldelta;
    QSpinBox *countSpin, *deltaSpin;
    QDialogButtonBox *buttonBox;

    DalgDialog(QWidget *parent);
};
