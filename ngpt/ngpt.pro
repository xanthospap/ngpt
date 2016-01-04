######################################################################
# Automatically generated by qmake (2.01a) Sat Jan 2 20:31:21 2016
######################################################################

QWT_INCLUDE      = /usr/local/qwt-6.1.3-svn/include
QWTPOLAR_INCLUDE = /usr/local/qwtpolar-1.1.1-svn/include
QWT_LIB          = /usr/local/qwt-6.1.3-svn/lib
QWTPOLAR_LIB     = /usr/local/qwtpolar-1.1.1-svn/lib

QT          += svg

TARGET       = polardemo

# Input
HEADERS += mainwindow.hpp pixmaps.hpp plot.hpp settingseditor.hpp
SOURCES += main.cpp mainwindow.cpp plot.cpp settingseditor.cpp

CONFIG         += qwt
CONFIG         += qwtpolar

INCLUDEPATH    += $${QWT_INCLUDE}
INCLUDEPATH    += $${QWTPOLAR_INCLUDE}
# INCLUDEPATH    += ../src /usr/include/qt5/QtWidgets /usr/include/qt5/QtPrintSupport
# INCLUDEPATH    += /usr/include/qwt
INCLUDEPATH += /usr/include/qt5/QtPrintSupport

LIBS           += -L$${QWTPOLAR_LIB} -lqwtpolar
LIBS           += -L$${QWT_LIB} -lqwt
LIBS           += -L../usr/local/lib/ -lngpt

QMAKE_CXXFLAGS += -std=c++11
