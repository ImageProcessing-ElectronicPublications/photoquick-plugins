HEADERS = threshold_bg_scale.h
SOURCES = threshold_bg_scale.cpp

TARGET  = $$qtLibraryTarget(threshold-bg-scale)
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

CONFIG -= debug_and_release
