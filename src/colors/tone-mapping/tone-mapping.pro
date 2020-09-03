HEADERS = tone_mapping.h
SOURCES = tone_mapping.cpp mantiuk06.cpp

TARGET  = $$qtLibraryTarget(tone-mapping)
DESTDIR = ../..
INCLUDEPATH += $$DESTDIR

TEMPLATE        = lib
CONFIG         += plugin
QMAKE_CXXFLAGS  = -std=c++11 -fopenmp
QMAKE_LFLAGS   += -s -lgomp
LIBS           +=

MOC_DIR =     $$DESTDIR/build
OBJECTS_DIR = $$DESTDIR/build

unix {
    INSTALLS += target
    target.path = /usr/local/share/photoquick/plugins
}

CONFIG -= debug_and_release
