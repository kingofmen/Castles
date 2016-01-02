#ifndef WARFAREWINDOW_HH
#define WARFAREWINDOW_HH

#include <QtGui>
#include <QObject>
#include "RiderGame.hh"
#include "Player.hh"
#include "Logger.hh"
#include "Action.hh"
#include "Hex.hh"
#include "GraphicsInfo.hh"
#include <iterator>
#include <vector>
#include <algorithm>

class ThreeDSprite;
using namespace std;

typedef void (GraphicsInfo::*TextFunc)(QTextStream&) const;

class TextInfoDisplay : public QLabel {
public:
  TextInfoDisplay (QWidget* p, TextFunc t);
  void setSelected (const GraphicsInfo* g) {gInfo = g;}
  void draw ();

private:
  const GraphicsInfo* gInfo;
  void (GraphicsInfo::*descFunc)(QTextStream&) const;
};

class UnitInterface : public QLabel {
  Q_OBJECT
  friend class StaticInitialiser;
public:
  UnitInterface (QWidget* p);
  void setUnit (MilUnit* m);
  void updateUnitInfo ();

public slots:
  void increasePriority (int direction);

signals:

private:
  QToolButton increasePriorityButton;
  QToolButton decreasePriorityButton;
  MilUnit* selectedUnit;
};

class EventList {
public:
  EventList (QWidget* p, int numEvents, int xcoord, int ycoord);
  ~EventList ();
  void setSelected (const GraphicsInfo* gi) {selected = gi;}
  void draw ();
private:
  const GraphicsInfo* selected;
  vector<QLabel*> events;
};

class CastleInterface : public QLabel {
  Q_OBJECT
  friend class StaticInitialiser; 
public:
  CastleInterface (QWidget* p);
  void setCastle (Castle* dat);
  void updateCastleInfo (); 
			   
public slots:
  void changeRecruitment (int direction);
  
signals: 

private:
  QToolButton increaseRecruitButton;
  QToolButton decreaseRecruitButton;
  Castle* castle;

  static map<MilUnitTemplate const* const, QIcon> icons; 
};

class VillageInterface : public QLabel {
  Q_OBJECT
  friend class StaticInitialiser; 
public:
  VillageInterface (QWidget* p);
  void setVillage (Village* dat) {village = dat;}
  void updateFarmInfo (); 
			   
public slots:
  void changeDrillLevel (int direction);
  
signals: 

private:
  QToolButton increaseDrillButton;
  QToolButton decreaseDrillButton;
  Village* village; 
};

class HexDrawer {
public:
  HexDrawer (QWidget* p);
  virtual ~HexDrawer (); 

  virtual void draw () = 0; 
  virtual Hex* findHex (double x, double y) = 0;
  virtual Vertex* findVertex (double x, double y) = 0;
  virtual Line* findLine (double x, double y) = 0;
  virtual void setTranslate (int x, int y) {translateX = x; translateY = y;}
  void zoom (int delta);
  void rotate (double amount) {radial += amount;} 
  void azimate (double amount); 
  
protected:
  QWidget* parent; 
  int translateX;
  int translateY;
  int zoomLevel;  // Distance of viewer from scene. Lower number is closer zoom. 
  double azimuth;
  double radial; 
};

class MapOverlay {
public:
  virtual void drawLine (LineGraphicsInfo const* dat) = 0;
};

class SupplyMode : public MapOverlay {
public:
  virtual void drawLine (LineGraphicsInfo const* dat); 
}; 

class GLDrawer : public HexDrawer, public QGLWidget {
  friend class StaticInitialiser; 
public:
  GLDrawer (QWidget* p);
  ~GLDrawer () {}
  virtual void draw (); 
  virtual Hex* findHex (double x, double y);
  virtual Vertex* findVertex (double x, double y);
  virtual Line* findLine (double x, double y);
  virtual void setTranslate (int x, int y);
  void setViewport (); 
  void assignColour (Player* p); 
  void setOverlayMode (MapOverlay* m) {overlayMode = m;} 
  
protected:
  void convertToOGL (double& x, double& y); 
  virtual void paintGL ();
  virtual void initializeGL ();
  virtual void resizeGL ();   
  
private:
  void drawCastle (Castle* castle) const;
  void drawLine (LineGraphicsInfo const* dat);
  void drawHex (HexGraphicsInfo const* dat);
  void drawSprites (const SpriteContainer* info, vector<int>& texts, double angle);  
  void drawMilUnit (MilUnit* unit, triplet center, double angle); 
  void drawVertex (VertexGraphicsInfo const* dat);
  void drawZone (int which); 
  ThreeDSprite* makeSprite (Object* info); 

  int* errors;
  GLuint* terrainTextureIndices;
  GLuint* zoneTextures;  // Zones get their own array because their generation creates new texture names.
  map<Player*, GLuint> playerToTextureMap; 
  
  ThreeDSprite* cSprite;
  ThreeDSprite* tSprite;
  ThreeDSprite* farmSprite;  

  MapOverlay* overlayMode; 
}; 



class WarfareWindow : public QMainWindow {
  Q_OBJECT
  friend class StaticInitialiser; 

public:
  WarfareWindow (QWidget* parent = 0); 
  ~WarfareWindow ();

  QPlainTextEdit* textWindow;
  void initialiseColours ();
  void clearGame (); 
  void newGame (string fname); 
  void chooseTask (string fname, int task); 
				  
public slots:
  void newGame ();  
  void loadGame ();  
  void saveGame ();
  void endTurn ();
  void message (QString m); 
  void setMapMode (int m); 
  void update ();
  void copyHistory (); 
  
signals:
  
protected:
  void paintEvent(QPaintEvent *event);
  void mouseReleaseEvent (QMouseEvent* event);
  void mousePressEvent (QMouseEvent* event);
  void keyReleaseEvent (QKeyEvent* event);
  void wheelEvent (QWheelEvent* event); 
private:
  GLDrawer* hexDrawer;
  TextInfoDisplay* selDrawer;
  EventList* histDrawer;
  EventList* marketDrawer;
  UnitInterface* unitInterface;
  CastleInterface* castleInterface;
  VillageInterface* villageInterface;   
  SupplyMode supplyMode; 
  
  void humanAction (Action& act); 
  bool turnEnded ();
  void endOfTurn ();
  void initialiseGraphics(); 
  void runNonHumans (); 
  Player* gameOver (); 

  void selectObject ();
  
  WarfareGame* currentGame;
  Hex* selectedHex;
  Line* selectedLine; 
  Vertex* selectedVertex; 
  
  int mouseDownX;
  int mouseDownY;

  QToolButton plainMapModeButton;
  QToolButton supplyMapModeButton;
  
  static map<int, Logger*> logs;
  static WarfareWindow* currWindow; 
};

#endif
