QT += core gui qml quick quickcontrols2

linux:!android: QT += dbus

CONFIG += c++17 release
TARGET = oma12c
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc

# Platform-specific output and packaging.
win32 {
    TARGET = OMA12C
}

macx {
    TARGET = OMA12C
    QMAKE_INFO_PLIST = macos/Info.plist
    CONFIG += app_bundle
}

android {
    TARGET = oma12c
    ANDROID_ABIS = armeabi-v7a arm64-v8a x86 x86_64
    ANDROID_MIN_SDK_VERSION = 26
    ANDROID_TARGET_SDK_VERSION = 34
    ANDROID_VERSION_CODE = 1
    ANDROID_VERSION_NAME = 1.0.0
    DISTFILES += \
        android/AndroidManifest.xml \
        android/build.gradle \
        android/gradle.properties
}
