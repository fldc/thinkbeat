QT += core gui qml quick quickcontrols2
CONFIG += c++17 release
TARGET = thinkbeat
TEMPLATE = app
HEADERS += src/backend.h src/pulse.h src/led.h src/theme.h src/translator.h
SOURCES += src/main.cpp src/backend.cpp src/pulse.cpp src/led.cpp src/theme.cpp src/translator.cpp
RESOURCES += src/resources.qrc
