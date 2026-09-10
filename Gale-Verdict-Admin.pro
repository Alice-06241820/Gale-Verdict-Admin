QT += widgets charts network

CONFIG += c++17

TARGET = Gale-Verdict-Admin
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    service/adminapiservice.cpp \
    pages/loginpage.cpp \
    pages/dashboardpage.cpp \
    pages/chargerspage.cpp \
    pages/stationspage.cpp \
    pages/userspage.cpp \
    ui/transientmessage.cpp

HEADERS += \
    mainwindow.h \
    model/adminmodels.h \
    service/adminapiservice.h \
    pages/loginpage.h \
    pages/dashboardpage.h \
    pages/chargerspage.h \
    pages/stationspage.h \
    pages/userspage.h \
    ui/transientmessage.h

RESOURCES += resources.qrc

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
