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
    void filterScaleX(int n);
    void filterXBR(int n);

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
    QComboBox *comboMethod;
    QSpinBox *spinMult;

    UpscaleDialog(QWidget *parent);
};

// XBR scaler
void xbr_filter_xbr2x(uint32_t* src, uint32_t *dst, int width, int height);
void xbr_filter_xbr3x(uint32_t* src, uint32_t *dst, int width, int height);
void xbr_filter_xbr4x(uint32_t* src, uint32_t *dst, int width, int height);

// ScaleX scaler
void scaler_scalex_2x(uint32_t* src, uint32_t *dst, int width, int height);
void scaler_scalex_3x(uint32_t* src, uint32_t *dst, int width, int height);
void scaler_scalex_4x(uint32_t* src, uint32_t *dst, int width, int height);
