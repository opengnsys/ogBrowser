TEMPLATE	= app
DESTDIR 	= ..

CONFIG		+= qt debug_and_release warn_on build_all static

QT += core gui webkit

MOC_DIR 	= ../.moc

CONFIG(debug, debug|release) {
    OBJECTS_DIR = ../.objs_d
    TARGET 	= browser_d
    LIBS 	+= -L.. ../libqtermwidget_d.a
} else {
    OBJECTS_DIR = ../.objs
    TARGET 	= browser
    LIBS 	+= -L.. ../libqtermwidget.a
}

SOURCES 	= main.cpp mainwindow.cpp

HEADERS = mainwindow.h

INCLUDEPATH 	= ../qtermwidget/src

#LIBS 		+= -L.. -lqtermwidget


	
