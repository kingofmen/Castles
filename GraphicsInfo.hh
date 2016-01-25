#ifndef GRAPHICS_INFO_HH
#define GRAPHICS_INFO_HH

#include <vector>
#include <QtOpenGL>
#include <QTextStream>
#include "UtilityFunctions.hh"
#include "ThreeDSprite.hh"
#include "Directions.hh" 
#include "GraphicsBridge.hh"

class MilStrength; 
class MilUnit;
class MilUnitTemplate; 
class Hex;
class HexGraphicsInfo; 
class Vertex;
class Line;
class Farmland;
class FieldStatus;
class Castle;
class Market;
class Village;

enum TerrainType {Mountain = 0, Hill, Plain, Wooded, Ocean, NoTerrain}; 

struct MilUnitSprite {
  ThreeDSprite* soldier;     // Figure for one man - will be drawn several times. 
  vector<doublet> positions; // Positions to draw the soldiers, relative to a central point.
};

class SpriteContainer {
public:

  struct spriterator {
    spriterator (int idx, SpriteContainer const* const b) : index(idx), boss(b) {}
    MilUnitSprite* operator* () {if ((int) boss->spriteIndices.size() <= index) return sprites[0]; return sprites[boss->spriteIndices[index]];}
    doublet getFormation () const {return boss->formation[index];}
    void operator++ () {index++;}
    bool operator!= (const spriterator& other) const {return index != other.index;}
    bool operator== (const spriterator& other) const {return index == other.index;}    
    
  private:
    int index;
    SpriteContainer const* const boss; 
  };

  spriterator start () const {return spriterator(0, this);}
  spriterator final () const {return spriterator(spriteIndices.size(), this);}
  
protected: 
  vector<int> spriteIndices;
  vector<doublet> formation;
  static vector<MilUnitSprite*> sprites;    
}; 

class FarmGraphicsInfo : public GraphicsInfo, public TextInfo, public GraphicsBridge<Farmland, FarmGraphicsInfo>, public SpriteContainer, public Iterable<FarmGraphicsInfo> {
  friend class StaticInitialiser;
  friend class HexGraphicsInfo; 
public:
  FarmGraphicsInfo (Farmland* f);
  virtual ~FarmGraphicsInfo ();
  static void updateFieldStatus (); 

  struct FieldInfo {
    friend class FarmGraphicsInfo; 
    FieldInfo (FieldShape s); 
    int getIndex () const;
    
    cpit begin () const {return shape.begin();}
    pit  begin ()       {return shape.begin();}
    cpit end () const {return shape.end();}
    pit  end ()       {return shape.end();}
    
  private:
    FieldShape shape;
    double area;
    FieldStatus const* status;
  };

  typedef vector<FieldInfo>::const_iterator cfit;
  typedef vector<FieldInfo>::iterator fit;
  cfit start () const {return fields.begin();}
  cfit final () const {return fields.end();}
  fit start () {return fields.begin();}
  fit final () {return fields.end();}
  
private:  
  double fieldArea ();   
  void generateShapes (HexGraphicsInfo* dat);
  vector<FieldInfo> fields;
  static vector<int> textureIndices; 
};

class VillageGraphicsInfo : public GraphicsInfo, public TextInfo, public GBRIDGE(Village), public SpriteContainer, public Iterable<VillageGraphicsInfo> {
  friend class StaticInitialiser;
  friend class HexGraphicsInfo; 
public:
  VillageGraphicsInfo (Village* f);
  virtual ~VillageGraphicsInfo ();
  int getHouses () const;
  static void updateVillageStatus (); 
  
  cpit startDrill () const {return exercis.begin();}
  cpit finalDrill () const {return exercis.end();}
  cpit startHouse () const {return village.begin();}
  cpit finalHouse () const {return village.end();}
  cpit startSheep () const {return pasture.begin();}
  cpit finalSheep () const {return pasture.end();}

private:  
  double fieldArea ();   
  void generateShapes (HexGraphicsInfo* dat);
  FieldShape exercis;
  FieldShape village;
  FieldShape pasture; 
  
  static unsigned int supplySpriteIndex;
  static int maxCows;
  static int suppliesPerCow; 
  static vector<doublet> cowPositions;
};

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

class CastleGraphicsInfo : public GraphicsInfo, public TextInfo, public GBRIDGE(Castle) {
public:
  CastleGraphicsInfo (Castle* castle);
  ~CastleGraphicsInfo ();

  double getAngle () const {return angle;}
private:
  double angle;
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

class MarketGraphicsInfo : public TBRIDGE(Market) {
public:
  MarketGraphicsInfo (Market* dat);
  ~MarketGraphicsInfo ();
};

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
