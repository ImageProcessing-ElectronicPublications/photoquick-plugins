#ifndef __XPNG_H
#define __XPNG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define XPNG_VERSION "1.5"
#define XPNG_URL "https://github.com/ImageProcessing-ElectronicPublications/xpng"
#define XPNG_BPP 4

typedef unsigned char png_byte;
typedef png_byte * png_bytep;

void xpng(png_bytep buffer, int32_t w, int32_t h, size_t pngsize, uint8_t clevel, int32_t radius);

#ifdef __cplusplus
}
#endif

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


class XPNGDialog : public QDialog
{
public:
    QGridLayout *gridLayout;
    QLabel *LevelLabel, *RadiusLabel;
    QSpinBox *LevelSpin, *RadiusSpin;
    QDialogButtonBox *buttonBox;

    XPNGDialog(QWidget *parent);
};

#endif /* __XPNG_H */
