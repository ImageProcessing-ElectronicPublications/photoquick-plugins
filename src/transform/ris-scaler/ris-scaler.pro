HEADERS = ris_scaler.h
SOURCES = ris_scaler.cpp lib/common_ris.c lib/gsample.c lib/hris.c lib/reduce.c

TARGET  = $$qtLibraryTarget(ris-scaler)
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
