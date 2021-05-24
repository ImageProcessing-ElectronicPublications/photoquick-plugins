HEADERS = deskew.h
SOURCES = deskew.cpp

TARGET  = $$qtLibraryTarget(deskew)
DESTDIR = ../..
INCLUDEPATH += $$DESTDIR

TEMPLATE        = lib
CONFIG         += plugin
QMAKE_CXXFLAGS  = -std=c++11 -fopenmp
QMAKE_LFLAGS   += -s
LIBS           += -lgomp

MOC_DIR =     $$DESTDIR/build
OBJECTS_DIR = $$DESTDIR/build

unix {
    INSTALLS += target
    target.path = /usr/local/share/photoquick/plugins
}

CONFIG -= debug_and_release debug
