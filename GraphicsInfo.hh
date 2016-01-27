#ifndef GRAPHICS_INFO_HH
#define GRAPHICS_INFO_HH

#include <vector>
#include <QtOpenGL>
#include <QTextStream>
#include "GraphicsBridge.hh"

enum TerrainType {Mountain = 0, Hill, Plain, Wooded, Ocean, NoTerrain}; 

class HexGraphicsInfo : public GraphicsInfo, public TextInfo, public GBRIDGE(Hex), public Iterable<HexGraphicsInfo> {
public:
  HexGraphicsInfo (Hex* h); 
  ~HexGraphicsInfo ();

  virtual void describe (QTextStream& str) const;  
  triplet getCoords (Vertices v) const;
  FarmGraphicsInfo const* getFarmInfo () const {return farmInfo;}
  VillageGraphicsInfo const* getVillageInfo () const {return villageInfo;}
  FieldShape getPatch (bool large = false); 
  TerrainType getTerrain () const {return terrain;}
  bool isInside (double x, double y) const;
  void setFarm (FarmGraphicsInfo* f);
  void setVillage (VillageGraphicsInfo* f);  
  static void getHeights (); 

  cpit startTrees () const {return trees.begin();}
  cpit finalTrees () const {return trees.end();}
  
private:
  void generateShapes ();
  double patchArea () const;
  
  TerrainType terrain;
  triplet cornerRight;
  triplet cornerRightDown;
  triplet cornerLeftDown;
  triplet cornerLeft;
  triplet cornerLeftUp;
  triplet cornerRightUp;  
  FarmGraphicsInfo* farmInfo;
  VillageGraphicsInfo* villageInfo;
  vector<FieldShape> spritePatches; // Places to put sprites - hand out to subordinates.
  vector<FieldShape> biggerPatches; // For larger shapes like drill grounds.   
  FieldShape trees; // Not a polygon.
  vector<int> treesPerField;
};

class VertexGraphicsInfo : public GraphicsInfo, public TextInfo, public GBRIDGE(Vertex), public Iterable<VertexGraphicsInfo> {
public:
  VertexGraphicsInfo (Vertex* v, HexGraphicsInfo const* hex, Vertices dir); 
  ~VertexGraphicsInfo ();

  typedef vector<VertexGraphicsInfo*>::const_iterator Iterator;
  virtual void describe (QTextStream& str) const;  
  triplet getCorner (int i) const {switch (i) {case 0: return corner1; case 1: return corner2; default: return corner3;}}
  bool isInside (double x, double y) const;
  
private:
  // Clockwise order, with corner1 having the lowest y-coordinate. 
  triplet corner1;
  triplet corner2;
  triplet corner3;
};


class LineGraphicsInfo : public GraphicsInfo, public TextInfo, public GBRIDGE(Line), public Iterable<LineGraphicsInfo> {
public:
  LineGraphicsInfo (Line* l, Vertices dir); 
  virtual ~LineGraphicsInfo ();  

  virtual void describe (QTextStream& str) const;  
  double  getAngle () const {return angle;}
  triplet getCorner (int which) const; 
  double  getFlowRatio () const {return flow * maxFlow;}
  double  getLossRatio () const {return loss * maxLoss;}
  bool    isInside (double x, double y) const;
  
  static void endTurn ();

private:
  double flow;
  double loss;
  double angle; 

  // Clockwise order 
  triplet corner1;
  triplet corner2;
  triplet corner3;
  triplet corner4;

  static double maxFlow;
  static double maxLoss;
};

class MilUnitGraphicsInfo : public GBRIDGE(MilUnit), public TextInfo, public SpriteContainer {
  friend class StaticInitialiser;
public:
  MilUnitGraphicsInfo (MilUnit* dat) : GBRIDGE(MilUnit)(dat), TextInfo() {}
  virtual ~MilUnitGraphicsInfo ();

  virtual void describe (QTextStream& str) const;
  string strengthString (string indent) const;
  void updateSprites (MilStrength* dat);

private:
  static map<MilUnitTemplate*, int> indexMap;
  static vector<vector<doublet> > allFormations;
};

class ZoneGraphicsInfo : public GraphicsInfo, public Iterable<ZoneGraphicsInfo>, public Numbered<ZoneGraphicsInfo> {
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

  gridIt gridBegin () {return grid.begin();}
  gridIt gridEnd   () {return grid.end();}

private:
  int badLine (triplet one, triplet two);  
  void gridAdd (triplet coords);  
  void recalc ();

  double** heightMap; 
  vector<vector<triplet> > grid; // Stores points to draw hex grid on terrain. 
};

// Following classes don't have a position, so don't descend from GraphicsInfo.

class PlayerGraphicsInfo {
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

#endif
