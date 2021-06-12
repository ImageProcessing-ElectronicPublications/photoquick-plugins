#pragma once
#include <QString>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
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


class GeoConformalDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *labelp, *labelr, *labeli, *labelm;
    QLineEdit *textp, *textr, *texti, *textm;
    QDialogButtonBox *buttonBox;
    QString sparams = "0;0;1;0";
    QString sregion = "0;0;100;100";
    QString siters = "10";
    QString smargin = "0";

    GeoConformalDialog(QWidget *parent);
};

class GeoConformalWarningDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *labelnc, *labelns;
    QLineEdit *textnc, *textns;
    QDialogButtonBox *buttonBox;
    QString snc, sns;

    GeoConformalWarningDialog(QWidget *parent);
};
