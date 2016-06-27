######################################################################
# Automatically generated by qmake (3.0) Mon Jun 27 00:33:07 2016
######################################################################

LIBS+=-lopengl32
LIBS+=-lglu32
QT+=opengl
CONFIG+=exceptions
QMAKE_CXXFLAGS+= -Wno-unused-local-typedefs
QMAKE_CXXFLAGS+=-std=c++11
TEMPLATE = app
TARGET = Castles
INCLUDEPATH += .

# Input
HEADERS += Action.hh \
           AgeTracker.hh \
           Building.hh \
           Calendar.hh \
           CastleWindow.hh \
           Directions.hh \
           EconActor.hh \
           glextensions.h \
           GraphicsBridge.hh \
           Hex.hh \
           Logger.hh \
           Market.hh \
           MilUnit.hh \
           Mirrorable.hh \
           Player.hh \
           RiderGame.hh \
           RoadButton.hh \
           StaticInitialiser.hh \
           StructUtils.hh \
           UtilityFunctions.hh \
           graphics/ThreeDSprite.hh \
           graphics/GeoGraphics.hh \
           graphics/PlayerGraphics.hh \
           graphics/UnitGraphics.hh \
           graphics/BuildingGraphics.hh
SOURCES += Action.cc \
           AgeTracker.cc \
           Building.cc \
           Calendar.cc \
           CastleWindow.cpp \
           Directions.cc \
           EconActor.cc \
           glextensions.cpp \
           GraphicsBridge.cc \
           Hex.cc \
           Logger.cc \
           Market.cc \
           MilUnit.cc \
           Mirrorable.cc \
           Player.cc \
           RiderGame.cpp \
           RoadButton.cc \
           StaticInitialiser.cc \
           UtilityFunctions.cc \
           graphics/ThreeDSprite.cc \
           graphics/GeoGraphics.cc \
           graphics/PlayerGraphics.cc \
           graphics/UnitGraphics.cc \
           graphics/BuildingGraphics.cc
