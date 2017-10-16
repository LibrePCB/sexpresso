#-------------------------------------------------
#
# Project created 2017-10-16
#
#-------------------------------------------------

TEMPLATE = lib
TARGET = sexpresso

# Set the path for the generated library
GENERATED_DIR = ../../generated

# Use common project definitions
include(../../common.pri)

QT -= core gui widgets

CONFIG -= qt app_bundle
CONFIG += staticlib

# suppress compiler warnings
CONFIG += warn_off

DEFINES += SEXPRESSO_OPT_OUT_PIKESTYLE

SOURCES += \
    sexpresso/sexpresso.cpp \

HEADERS += \
    sexpresso/sexpresso.hpp \
