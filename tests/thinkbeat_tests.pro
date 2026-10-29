QT += core gui qml quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = thinkbeat_tests
TEMPLATE = app
INCLUDEPATH += ../src
HEADERS += ../src/backend.h ../src/pulse.h ../src/led.h ../src/theme.h ../src/translator.h
SOURCES += thinkbeat_tests.cpp ../src/backend.cpp ../src/pulse.cpp ../src/led.cpp ../src/theme.cpp ../src/translator.cpp
RESOURCES += ../src/resources.qrc
