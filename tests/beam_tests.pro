QT += core gui quick quickcontrols2 testlib dbus network
CONFIG += c++17 testcase
TARGET = beam_tests
INCLUDEPATH += ../src
HEADERS += ../src/mpris.h ../src/backend.h ../src/theme.h ../src/translator.h
SOURCES += beam_tests.cpp ../src/mpris.cpp ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp
RESOURCES += ../src/resources.qrc
