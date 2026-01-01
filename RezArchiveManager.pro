QT       += core gui widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

TARGET = DHNP
TEMPLATE = app

SOURCES += \
    Dtxformat.cpp \
    Ltbformat.cpp \
    main.cpp \
    mainwindow.cpp \
    pcxformat.cpp \
    rezarchive.cpp

HEADERS += \
    Dtxformat.h \
    Ltbformat.h \
    mainwindow.h \
    pcxformat.h \
    rezarchive.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Platform specific settings
win32
{
    RC_ICONS = icon.ico
# QMAKE_LFLAGS += /MANIFESTUAC:level='asInvoker'
}

unix:!macx
{
   # QMAKE_LFLAGS += -no-pie
}

macx
{
    ICON = icon.icns
}

# Build directory structure
OBJECTS_DIR = build/obj
MOC_DIR = build/moc
RCC_DIR = build/rcc
UI_DIR = build/ui

# Optimize for release
CONFIG(release, debug|release)
{
    DEFINES += QT_NO_DEBUG_OUTPUT
    QMAKE_CXXFLAGS_RELEASE -= -O2
    QMAKE_CXXFLAGS_RELEASE += -O3
}

RESOURCES += \
    resources.qrc

