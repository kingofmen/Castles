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

class SelectedDrawer : public QLabel {
public:
  SelectedDrawer (QWidget* p);
  void setSelected (const GraphicsInfo* g) {gInfo = g;} 
  void draw (); 
  //void setSelected (Hex* s);
  //void setSelected (Line* s);
  //void setSelected (Vertex* s); 
  
private:
  const GraphicsInfo* gInfo; 
}; 

class UnitInterface : public QLabel {
  Q_OBJECT
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
  MilUnit* unit; 
};

class CastleInterface : public QLabel {
  Q_OBJECT
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
};

class FarmInterface : public QLabel {
  Q_OBJECT
public:
  FarmInterface (QWidget* p);
  void setFarm (Farmland* dat);
  void updateFarmInfo (); 
			   
public slots:
  void changeDrillLevel (int direction);
  
signals: 

private:
  QToolButton increaseDrillButton;
  QToolButton decreaseDrillButton;
  Farmland* farm; 
};

class HexDrawer {
public:
  HexDrawer (QWidget* p);
  virtual ~HexDrawer (); 

  virtual void draw () = 0; 
  virtual Hex* findHex (double x, double y) = 0;
  virtual Vertex* findVertex (double x, double y) = 0;
  virtual Line* findLine (double x, double y) = 0;
  virtual void loadSprites () = 0; 
  virtual void setTranslate (int x, int y) {translateX = x; translateY = y;}
  void zoom (int delta);
  void rotate (double amount) {radial += amount;} 
  void azimate (double amount); 
  
protected:
  QWidget* parent; 
  int translateX;
  int translateY;
  int zoomLevel;
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
  virtual void loadSprites ();
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
  int assignTextureIndex (); 
  int loadTexture (std::string fname, QColor backup);
  //void loadTexture (int texName, QColor backup, std::string fname);
  void drawCastle (Castle* castle, LineGraphicsInfo const* dat); 
  void drawLine (LineGraphicsInfo const* dat);
  void drawHex (HexGraphicsInfo const* dat);
  void drawVertex (VertexGraphicsInfo const* dat);
  void drawZone (int which); 
  ThreeDSprite* makeSprite (Object* info); 

  GLuint* textureIDs;
  int assignedTextures; 
  int* errors;
  int* terrainTextureIndices;
  int* castleTextureIndices;
  int* knightTextureIndices;
  int* zoneTextures;  // Zones get their own array because their generation creates new texture names. 
  const int numTextures;
  std::map<Player*, int> playerToTextureMap; 
  
  ThreeDSprite* cSprite;
  ThreeDSprite* kSprite;

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
  void newGame (std::string fname); 
  void chooseTask (std::string fname, int task); 
				  
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
  SelectedDrawer* selDrawer;
  UnitInterface* unitInterface;
  CastleInterface* castleInterface;
  FarmInterface* farmInterface;   
  SupplyMode supplyMode; 
  
  void humanAction (Action& act); 
  bool turnEnded ();
  void endOfTurn ();
  void runNonHumans (); 
  Player* gameOver (); 

  void selectObject ();
  
  static char* popText (PopUnit* dat); 
  
  WarfareGame* currentGame;
  Player* currentPlayer; 
  Hex* selectedHex;
  Line* selectedLine; 
  Vertex* selectedVertex; 
  
  int mouseDownX;
  int mouseDownY;

  QToolButton plainMapModeButton;
  QToolButton supplyMapModeButton;
  
  static std::map<int, Logger*> logs;
  static WarfareWindow* currWindow; 
};

#endif
