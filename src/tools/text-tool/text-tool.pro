HEADERS = text_tool.h
SOURCES = text_tool.cpp
FORMS += $$files(*.ui)
RESOURCES += $$files(*.qrc)

TARGET  = $$qtLibraryTarget(text-tool)
DESTDIR = ../..
INCLUDEPATH += $$DESTDIR

TEMPLATE        = lib
CONFIG         += plugin
QMAKE_CXXFLAGS  = -std=c++11 -fopenmp
QMAKE_LFLAGS   += -s
LIBS           += -lgomp
QT             += widgets

MOC_DIR =     $$DESTDIR/build
RCC_DIR =     $$DESTDIR/build
OBJECTS_DIR = $$DESTDIR/build
UI_DIR  =     $$DESTDIR/build

unix {
    INSTALLS += target
    target.path = /usr/local/share/photoquick/plugins
}

CONFIG -= debug_and_release debug
