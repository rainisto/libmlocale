TEMPLATE = app
TARGET = multipleInstances
CONFIG += meegotouch

# Input
SOURCES += main.cpp

# The following lines are only to allow building the
# example inside the source tree without installing
# libm first:
INCLUDEPATH += ../../src/include
QMAKE_LIBDIR += ../../lib/
LIBS += -lmeegotouchcore
