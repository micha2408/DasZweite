QT -= gui core

CONFIG += c++11 console
CONFIG -= app_bundle

TARGET = topEthernet

CONFIG(debug, debug|release) {
DESTDIR = $$absolute_path(../../emugesamt/Debug)
} else {
DESTDIR = $$absolute_path(../../emugesamt/Release)
}

contains(QT_ARCH, i386) {
    message("using 32-bit libraries")
    LIBPATH = "$$getenv(WindowsSdkDir)Lib\\$$WINDOWS_TARGET_PLATFORM_VERSION\\um\\x86"
    # QMAKE_TARGET_PRODUCT = "topEthernet (32Bit)"
} else {
    message("using 64-bit libraries")
    LIBPATH = "$$getenv(WindowsSdkDir)Lib\\$$WINDOWS_TARGET_PLATFORM_VERSION\\um\\x64"
    DESTDIR = $$DESTDIR"64"
    # QMAKE_TARGET_PRODUCT = "topEthernet (64Bit)"
}
message("Building: "$$DESTDIR/$$TARGET".exe")
gcc:PREFIX = lib
msvc:QMAKE_CFLAGS_DEBUG -= -MDd
msvc:QMAKE_CXXFLAGS_DEBUG -= -MDd
msvc:QMAKE_CFLAGS_DEBUG += -MTd
msvc:QMAKE_CXXFLAGS_DEBUG += -MTd


win32:DEFINES += WIN32 NDEBUG _CONSOLE _WINDOWS FREMDLING
win32:LIBS += $$LIBPATH/Ws2_32.lib $$LIBPATH/winmm.lib \
              $$DESTDIR/$$PREFIX"topo_component_lib.lib" \
              $$DESTDIR/$$PREFIX"shared_component_lib.lib"

DEFINES -= UNICODE
#VERSION = 0.0.0.1
MSVC_VER = 16.0
ICON = top.ico
#DEFINES += APP_VERSION=\"\\\"$$VERSION\\\"\" \


#win32:DEFINES += APP_BUILDDATE=\"\\\"$$system(date /T)\\\"\" _TIMEZONE_DEFINED
win32:DEFINES += WIN32 NDEBUG _CONSOLE _WINDOWS FREMDLING

unix:DEFINES += APP_BUILDDATE=\"\\\"$$system(date +%F)\\\"\"

INCLUDEPATH = ../common\
              ../TopMan\
              ../../shared/include\
              ../../getopt\
              ../../../../../../Hackbrett/_section/targettools/TUSim/_inc\
              ../../HANDLER/IO_MM_ETHERNET

SOURCES += \
        TopCommand.cpp \
        TopConsole.cpp \
        TopPipe.cpp \
        Topologie.cpp \
        topEthernet.cpp

HEADERS += \
        Interfaces.hpp \
        TopCommand.hpp \
        TopConsole.hpp \
        TopPipe.hpp \
        Topologie.hpp \
        rfc1006_private.h \
        topEthernet.hpp

# Default rules for deployment.
#qnx: target.path = /tmp/$${TARGET}/bin
#else: unix:!android: target.path = /opt/$${TARGET}/bin
#!isEmpty(target.path): INSTALLS += target

