QT += gui core

CONFIG += c++11

TARGET = TransformFeedback
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

INCLUDEPATH += $$PWD/../glm/glm

SOURCES += main.cpp \
    TransformFeedback.cpp    

HEADERS += \
    TransformFeedback.h    

OTHER_FILES += \
    fshader.txt \
    vshader.txt

RESOURCES += \
    shaders.qrc

DISTFILES += \
    fshader.txt \
    vshader.txt
