QT += core gui qml quick quickcontrols2 dbus network
CONFIG += c++17 release
TARGET = beam
TEMPLATE = app
HEADERS += src/mpris.h src/backend.h src/theme.h src/translator.h
SOURCES += src/mpris.cpp src/main.cpp src/backend.cpp src/theme.cpp src/translator.cpp
RESOURCES += src/resources.qrc

HEADERS += src/queue.h
SOURCES += src/queue.cpp

HEADERS += src/outputdriver.h src/castoutput.h
SOURCES += src/castoutput.cpp
