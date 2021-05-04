HEADERS = photo_frame.h
SOURCES = photo_frame.cpp
FORMS += $$files(*.ui)

TARGET  = $$qtLibraryTarget(photo-frame)
DESTDIR = ../..
INCLUDEPATH += $$DESTDIR

TEMPLATE        = lib
CONFIG         += plugin
QMAKE_CXXFLAGS  = -std=c++11
QMAKE_LFLAGS   += -s
LIBS           +=

UI_DIR  =     $$DESTDIR/build
MOC_DIR =     $$DESTDIR/build
OBJECTS_DIR = $$DESTDIR/build

unix {
    INSTALLS += target
    target.path = /usr/local/share/photoquick/plugins
}

CONFIG -= debug_and_release debug
