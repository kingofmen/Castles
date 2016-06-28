######################################################################
# Automatically generated by qmake (3.0) Tue Jun 28 01:06:48 2016
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
HEADERS += AgeTracker.hh \
           Calendar.hh \
           CastleWindow.hh \
           Directions.hh \
           glextensions.h \
           Logger.hh \
           Mirrorable.hh \
           RiderGame.hh \
           RoadButton.hh \
           StaticInitialiser.hh \
           StructUtils.hh \
           UtilityFunctions.hh \
           game/Hex.hh \
           game/Building.hh \
           game/Market.hh \
           game/EconActor.hh \
           graphics/GraphicsBridge.hh \
           graphics/ThreeDSprite.hh \
           /Directions.hh \
           graphics/GeoGraphics.hh \
           game/Player.hh \
           graphics/PlayerGraphics.hh \
           game/Action.hh \
           graphics/BuildingGraphics.hh \
           game/MilUnit.hh \
           graphics/UnitGraphics.hh
SOURCES += AgeTracker.cc \
           Calendar.cc \
           CastleWindow.cpp \
           Directions.cc \
           glextensions.cpp \
           Logger.cc \
           Mirrorable.cc \
           RiderGame.cpp \
           RoadButton.cc \
           StaticInitialiser.cc \
           UtilityFunctions.cc \
           game/Hex.cc \
           game/Building.cc \
           game/Market.cc \
           game/EconActor.cc \
           graphics/GraphicsBridge.cc \
           graphics/ThreeDSprite.cc \
           graphics/GeoGraphics.cc \
           game/Player.cc \
           graphics/PlayerGraphics.cc \
           game/Action.cc \
           graphics/BuildingGraphics.cc \
           game/MilUnit.cc \
           graphics/UnitGraphics.cc
