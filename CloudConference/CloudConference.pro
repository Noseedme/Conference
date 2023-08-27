QT       += core gui multimedia multimediawidgets network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    audioInput.cpp \
    audioOutput.cpp \
    chatMessage.cpp \
    logQueue.cpp \
    main.cpp \
    myTcpSocket.cpp \
    myTextEdit.cpp \
    netHeader.cpp \
    partner.cpp \
    recvSolve.cpp \
    screen.cpp \
    sendImg.cpp \
    sendText.cpp \
    widget.cpp

HEADERS += \
    audioInput.h \
    audioOutput.h \
    chatMessage.h \
    logQueue.h \
    myTcpSocket.h \
    myTextEdit.h \
    netHeader.h \
    partner.h \
    recvSolve.h \
    screen.h \
    sendImg.h \
    sendText.h \
    widget.h

FORMS += \
    widget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    src.qrc
