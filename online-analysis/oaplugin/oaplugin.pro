######################################################################
# Automatically generated by qmake (3.0) Fri Jun 19 08:50:37 2015
######################################################################

TEMPLATE = lib
CONFIG += plugin debug_and_release c++11
TARGET = oaplugin
QT -= gui

OBJECTS_DIR = build
DESTDIR = lib
INCLUDEPATH += . ./include ../include /usr/local/include
LIBS += -L/usr/local/lib -larmadillo -llapack -L../lib -lonline-analysis
QMAKE_CXXFLAGS += -std=c++11

# Input
SOURCES += src/oatemporalwn.cpp
HEADERS += include/oatemporalwn.h