#ifndef GRAPHICS_INFO_HH
#define GRAPHICS_INFO_HH

#include <vector>
#include <QtOpenGL>
#include <QTextStream>
#include "UtilityFunctions.hh"
#ifndef Q_MOC_RUN // Hackish workaround to avoid BOOST_JOIN parsing issue. 
#include "boost/geometry/geometry.hpp" 
#endif 

class MilUnit;
class Hex;
class Vertex;
class Line; 
class Farmland; 

enum TerrainType {Mountain = 0, Hill, Plain, Forest, Ocean, NoTerrain}; 
enum Direction {NorthWest = 0, North, NorthEast, SouthEast, South, SouthWest, NoDirection};
enum Vertices {LeftUp = 0, RightUp, Right, RightDown, LeftDown, Left, NoVertex}; 


class GraphicsInfo {
  friend class StaticInitialiser; 
public:
  GraphicsInfo ();
  ~GraphicsInfo ();

  triplet getPosition () const {return position;}
  virtual void describe (QTextStream& /*str*/) const {} 
  int getZone () const {return 0;}
  static pair<double, double> getTexCoords (triplet gameCoords, int zone); 

  static int getHeight (int x, int y);
  static void getHeightMapCoords (int& hexX, int& hexY, Vertices dir); 

  static const int zoneSize = 512; // Size in pixels (for internal purposes, not on the screen). 
  
protected:
  triplet position;
  static int zoneSide; // Size in hexes
  static double* heightMap;

  static const double xIncrement;
  static const double yIncrement;
  static const double xSeparation;
  static const double ySeparation;
  static const double zSeparation;
  static const double zOffset; 

  
private:

};

class FarmGraphicsInfo : public GraphicsInfo {
public:
  FarmGraphicsInfo (Farmland* f);
  ~FarmGraphicsInfo ();

  Farmland* getFarm () {return myFarm;}
  
private:
  void generateShapes ();

  
  
  Farmland* myFarm; 
};

class HexGraphicsInfo : public GraphicsInfo {
public:
  HexGraphicsInfo (Hex* h); 
  ~HexGraphicsInfo ();

  typedef vector<HexGraphicsInfo*>::const_iterator Iterator;
  
  virtual void describe (QTextStream& str) const;  
  triplet getCoords (Vertices v) const;
  Hex* getHex () const {return myHex;} 
  TerrainType getTerrain () const {return terrain;}
  bool isInside (double x, double y) const;
  static Iterator begin () {return allHexGraphics.begin();}
  static Iterator end   () {return allHexGraphics.end();}
  static void getHeights (); 
  
private:
  TerrainType terrain;
  Hex* myHex;
  triplet cornerRight;
  triplet cornerRightDown;
  triplet cornerLeftDown;
  triplet cornerLeft;
  triplet cornerLeftUp;
  triplet cornerRightUp;  

  static vector<HexGraphicsInfo*> allHexGraphics; 
};

class VertexGraphicsInfo : public GraphicsInfo {
public:
  VertexGraphicsInfo (Vertex* v, HexGraphicsInfo const* hex, Vertices dir); 
  ~VertexGraphicsInfo ();

  typedef vector<VertexGraphicsInfo*>::const_iterator Iterator;
  virtual void describe (QTextStream& str) const;  
  triplet getCorner (int i) const {switch (i) {case 0: return corner1; case 1: return corner2; default: return corner3;}}
  Vertex* getVertex () const {return myVertex;} 
  bool isInside (double x, double y) const; 
  static Iterator begin () {return allVertexGraphics.begin();}
  static Iterator end   () {return allVertexGraphics.end();}
  
private:
  // Clockwise order, with corner1 having the lowest y-coordinate. 
  triplet corner1;
  triplet corner2;
  triplet corner3;

  Vertex* myVertex;

  static vector<VertexGraphicsInfo*> allVertexGraphics; 
};


class LineGraphicsInfo : public GraphicsInfo {
public:
  LineGraphicsInfo (Line* l, Vertices dir); 
  ~LineGraphicsInfo ();  

  typedef vector<LineGraphicsInfo*>::const_iterator Iterator;
  
  void addCastle (HexGraphicsInfo const* supportInfo);
  virtual void describe (QTextStream& str) const;  
  double  getAngle () const {return angle;}
  void    getCastlePosition (double& xpos, double& ypos, double& zpos) const; 
  triplet getCorner (int which) const; 
  double  getFlowRatio () const {return flow * maxFlow;}
  Line*   getLine () const {return myLine;} 
  double  getLossRatio () const {return loss * maxLoss;}
  triplet getNormal () const {return normal;}; 
  void traverseSupplies (double amount, double loss);
  bool isInside (double x, double y) const; 
  
  static void endTurn ();
  static Iterator begin () {return allLineGraphics.begin();}
  static Iterator end   () {return allLineGraphics.end();}
  
private:
  double flow;
  double loss;
  double angle; 

  double castleX;
  double castleY; 

  // Clockwise order 
  triplet corner1;
  triplet corner2;
  triplet corner3;
  triplet corner4;
  triplet normal; 

  Line* myLine; 
  
  static double maxFlow;
  static double maxLoss;
  static vector<LineGraphicsInfo*> allLineGraphics;
}; 

class PlayerGraphicsInfo { // Doesn't have a position, so doesn't descend from GraphicsInfo
  friend class StaticInitialiser; 
public:
  PlayerGraphicsInfo ();
  ~PlayerGraphicsInfo ();

  int getRed () const {return qRed(colour);}
  int getGreen () const {return qGreen(colour);}
  int getBlue () const {return qBlue(colour);}  
  
private:
  QRgb colour;
  GLuint flag_texture_id; 
};

class MilUnitGraphicsInfo : public GraphicsInfo {
public:

  MilUnitGraphicsInfo (MilUnit* dat) : unit(dat) {}
  ~MilUnitGraphicsInfo () {}
  virtual void describe (QTextStream& str) const;
  string strengthString (string indent) const;  
private:

  MilUnit* unit; 
};

class ZoneGraphicsInfo : public GraphicsInfo {
  friend class StaticInitialiser; 
public:
  ZoneGraphicsInfo (); 
  ~ZoneGraphicsInfo () {}

  typedef vector<vector<triplet> >::iterator gridIt;
  typedef vector<triplet>::iterator hexIt; 
  
  double minX;
  double minY;
  double maxX;
  double maxY;  
  double width;
  double height;

  void addHex (HexGraphicsInfo* hex);
  void addLine (LineGraphicsInfo* lin);
  void addVertex (VertexGraphicsInfo* vex);
  double getHeight (unsigned int x, unsigned int y) {return heightMap[x][y];}
  double calcHeight (double x, double y);
  static void calcGrid (); 
  static ZoneGraphicsInfo* getZoneInfo (unsigned int which) {if (which >= allZoneGraphics.size()) return 0; return allZoneGraphics[which];}

  gridIt gridBegin () {return grid.begin();}
  gridIt gridEnd   () {return grid.end();}

private:
  int badLine (triplet one, triplet two);  
  void gridAdd (triplet coords);  
  void recalc ();
  
  double** heightMap; 
  vector<vector<triplet> > grid; // Stores points to draw hex grid on terrain. 
  
  static vector<ZoneGraphicsInfo*> allZoneGraphics; 
};


#endif
