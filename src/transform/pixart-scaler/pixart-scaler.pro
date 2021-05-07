HEADERS = pixart_scaler.h
SOURCES = pixart_scaler.cpp scaler_xbr.c scaler_hqx.c scaler_scalex.c

TARGET  = $$qtLibraryTarget(pixart-scaler)
DESTDIR = ../..
INCLUDEPATH += $$DESTDIR

TEMPLATE        = lib
CONFIG         += plugin
QMAKE_CXXFLAGS  = -std=c++11
QMAKE_LFLAGS   += -s
LIBS           +=

MOC_DIR =     $$DESTDIR/build
OBJECTS_DIR = $$DESTDIR/build

unix {
    INSTALLS += target
    target.path = /usr/local/share/photoquick/plugins
}

CONFIG -= debug_and_release debug
