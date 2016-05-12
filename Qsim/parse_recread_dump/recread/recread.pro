#-------------------------------------------------
#
# Project created by QtCreator 2016-05-11T16:55:10
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = recread
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

DEFINES += ON_QT_PLATFORM

INCLUDEPATH += ../../../Prog/Project/MainProject/ \
               ../../../Prog/Project/MainProject/func/  \
               ../../../Prog/Project/MainProject/graphic_lib/ \
               ../../../Qsim/simu_hygro/simuhygro


HEADERS  += mainw.h \
    stm32f10x.h \
    ../../../Prog/Project/MainProject/typedefs.h \
    hw_stuff.h \
    ../../../Prog/Project/MainProject/func/events_ui.h \
    graph_disp.h \
    ../../../Prog/Project/MainProject/func/ui.h \
    ../../../Prog/Project/MainProject/func/ui_internals.h \
    ../../../Prog/Project/MainProject/func/core.h \
    ../../../Prog/Project/MainProject/func/ui_elements.h \
    ../../../Prog/Project/MainProject/func/eeprom_spi.h \
    ../../../../../../000_SoftOnly/MyGraphicLibrary/MyGraphicLibrary/graphic_lib_app/graphic_lib/graphic_lib.h \
    ../../../Prog/Project/MainProject/graphic_lib_user.h \
    ../../../Prog/Project/MainProject/func/ui_graphics.h \
    ../../../Prog/Project/MainProject/func/dispHAL.h \
    ../../../Prog/Project/MainProject/func/utilities.h \
    serial_port/MSerialPort_Global.hpp \
    serial_port/MSerialPort.hpp \
    serial_port/com_link.h

SOURCES += main.cpp
