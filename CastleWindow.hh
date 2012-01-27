#ifndef WARFAREWINDOW_HH
#define WARFAREWINDOW_HH

#include <QtGui>
#include <QObject> 
#include "RiderGame.hh"
#include "Player.hh" 
#include "Logger.hh" 
#include "Action.hh" 
#include "Hex.hh"
#include <QtOpenGL>
#include <iterator>
#include <vector>
#include <algorithm> 
#include "boost/tuple/tuple.hpp"

class ThreeDSprite; 

template <class T> struct DrawInfo {
  typename T::Iterator start;
  typename T::Iterator final; 
  T* selected; 
};

class SelectedDrawer : public QLabel {
public:
  SelectedDrawer (QWidget* p);
  void setSelected (Hex* s);
  void setSelected (Line* s);
  void setSelected (Vertex* s); 
  
private:

}; 

class HexDrawer {
public:
  HexDrawer (QWidget* p);
  virtual ~HexDrawer () {}

  virtual void draw (DrawInfo<Hex>* hexes, DrawInfo<Vertex>* verts, DrawInfo<Line>* lines) = 0; 
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

class GLDrawer : public HexDrawer, public QGLWidget {
public:
  GLDrawer (QWidget* p);
  ~GLDrawer () {}
  virtual void draw (DrawInfo<Hex>* h, DrawInfo<Vertex>* v, DrawInfo<Line>* l);
  virtual Hex* findHex (double x, double y);
  virtual Vertex* findVertex (double x, double y);
  virtual Line* findLine (double x, double y);
  virtual void loadSprites ();
  virtual void setTranslate (int x, int y); 
  void assignColour (Player* p); 
  void clearColours (); 
  
protected:
  typedef boost::tuple<std::pair<double, double>, std::pair<double, double>, std::pair<double, double> > Triangle; 
  Triangle vertexTriangle (Vertex* vex) const;
  static bool intersect (double line1x1, double line1y1, double line1x2, double line1y2,
			 double line2x1, double line2y1, double line2x2, double line2y2);
  void convertToOGL (double& x, double& y); 
  virtual void paintGL ();
  virtual void initializeGL ();
  virtual void resizeGL ();   
  
private:
  std::pair<double, double> hexCenter (int x, int y) const;
  std::pair<double, double> vertexCenter (Vertex* dat) const;
  std::pair<double, double> vertexCoords (int x, int y, Hex::Vertices dir) const;
  void loadTexture (const char* fname, QColor backup, int idx);
  void loadTexture (int texName, QColor backup, std::string fname);
  void drawLine (Line* dat);
  void drawHex (Hex* dat);
  void drawVertex (Vertex* dat); 
  ThreeDSprite* makeSprite (Object* info); 
  
  DrawInfo<Hex>* hexes;
  DrawInfo<Vertex>* vexes;
  DrawInfo<Line>* lines; 
  GLuint* textureIDs;
  int* errors;
  int* terrainTextureIndices;
  int* castleTextureIndices;
  int* knightTextureIndices; 
  const int numTextures;
  std::map<Player*, double*> colourmap;
  std::map<Player*, int> playerToTextureMap; 
  
  ThreeDSprite* cSprite;
  ThreeDSprite* kSprite;
}; 


class WarfareWindow : public QMainWindow {
  Q_OBJECT
  
public:
  WarfareWindow (QWidget* parent = 0); 
  ~WarfareWindow ();

  QPlainTextEdit* textWindow;
  void update (); 			    
  void initialiseColours ();
  void clearGame (); 
  void newGame (std::string fname); 
		   
public slots:
  void newGame ();  
  void loadGame ();  
  void saveGame ();
  void endTurn ();
  void message (QString m); 
  
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
  
  void humanAction (Action& act); 
  bool turnEnded ();
  void endOfTurn ();
  void runNonHumans (); 
  Player* gameOver (); 
  
  static char* popText (PopUnit* dat); 
  
  WarfareGame* currentGame;
  Player* currentPlayer; 
  Hex* selectedHex;
  Line* selectedLine; 
  Vertex* selectedVertex; 
  
  int mouseDownX;
  int mouseDownY;
  
  static std::map<int, Logger*> logs;
};

#endif
