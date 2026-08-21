QT += core gui testlib dbus
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_oma12c

INCLUDEPATH += ../src
SOURCES += \
    tst_oma12c.cpp \
    ../src/backend.cpp
HEADERS += \
    ../src/backend.h
