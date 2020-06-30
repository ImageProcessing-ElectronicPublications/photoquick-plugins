#pragma once
#include "plugin.h"


class FilterPlugin : public QObject, Plugin
{
    Q_OBJECT
    Q_INTERFACES(Plugin)

public:
    QStringList menuItems();
    void filterScaleX(int n);
    void filterXBR(int n);

public slots:
    void handleAction(QAction *action, int type);
    void filterScale2x();
    void filterScale3x();
    void filterScale4x();
    void filterXBR2x();
    void filterXBR3x();
    void filterXBR4x();

signals:
    void imageChanged();
    void optimumSizeRequested();
    void sendNotification(QString title, QString message);
};

// XBR scaler
void xbr_filter_xbr2x(uint32_t* src, uint32_t *dst, int width, int height);
void xbr_filter_xbr3x(uint32_t* src, uint32_t *dst, int width, int height);
void xbr_filter_xbr4x(uint32_t* src, uint32_t *dst, int width, int height);

// ScaleX scaler
void scaler_scalex_2x(uint32_t* src, uint32_t *dst, int width, int height);
void scaler_scalex_3x(uint32_t* src, uint32_t *dst, int width, int height);
void scaler_scalex_4x(uint32_t* src, uint32_t *dst, int width, int height);
