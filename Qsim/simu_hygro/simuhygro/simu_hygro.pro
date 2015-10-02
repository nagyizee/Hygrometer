#-------------------------------------------------
#
# Project created by QtCreator 2013-04-09T18:05:58
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = simu_hygro
TEMPLATE = app

DEFINES += ON_QT_PLATFORM

INCLUDEPATH += ../../../Prog/Project/MainProject/ \
               ../../../Prog/Project/MainProject/func/  \
               ../../../../../../000_SoftOnly/MyGraphicLibrary/MyGraphicLibrary/graphic_lib_app/graphic_lib/

SOURCES += main.cpp\
        mainw.cpp \
    hw_wrapper.cpp \
    ../../../Prog/Project/MainProject/func/events_ui.c \
    graph_disp.cpp \
    ../../../Prog/Project/MainProject/mainapp.c \
    ../../../Prog/Project/MainProject/func/ui.c \
    ../../../Prog/Project/MainProject/func/core.c \
    ../../../Prog/Project/MainProject/func/ui_elements.c \
    ../../../../../../000_SoftOnly/MyGraphicLibrary/MyGraphicLibrary/graphic_lib_app/graphic_lib/graphic_lib.c \
    ../../../Prog/Project/MainProject/func/ui_graphics.c \
    ../../../Prog/Project/MainProject/func/utilities.c

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
    ../../../Prog/Project/MainProject/func/utilities.h

FORMS    += mainw.ui \
            graph.ui
