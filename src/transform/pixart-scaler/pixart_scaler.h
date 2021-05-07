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

#ifdef __cplusplus
extern "C" {
#endif

// XBR scaler
    void xbr_filter( uint32_t *src, uint32_t *dst, int inWidth, int inHeight, int scaleFactor);

// HQX scaler
    void hqx(uint32_t * sp,  uint32_t * dp, int Xres, int Yres, int scalefactor);

// ScaleX scaler
    void scaler_scalex(uint32_t * sp,  uint32_t * dp, int Xres, int Yres, int scalefactor);

#ifdef __cplusplus
}
#endif
