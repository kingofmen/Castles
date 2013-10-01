#ifndef GRAPHICS_INFO_HH
#define GRAPHICS_INFO_HH

#include <vector>
#include <QtOpenGL>
#include <QTextStream>
#include "UtilityFunctions.hh"
#include "Building.hh"
#include "ThreeDSprite.hh"
#include "Directions.hh" 

class MilStrength; 
class MilUnit;
class MilUnitTemplate; 
class Hex;
class HexGraphicsInfo; 
class Vertex;
class Line; 

enum TerrainType {Mountain = 0, Hill, Plain, Forest, Ocean, NoTerrain}; 


class GraphicsInfo {
  friend class StaticInitialiser; 
public:
  GraphicsInfo ();
  ~GraphicsInfo ();

  typedef vector<triplet> FieldShape;  
  typedef FieldShape::const_iterator cpit;
  typedef FieldShape::iterator pit;      
  
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
  friend class StaticInitialiser;
  friend class HexGraphicsInfo; 
public:
  FarmGraphicsInfo (Farmland* f);
  ~FarmGraphicsInfo ();
  Farmland* getFarm () const {return myFarm;}
  static void updateFieldStatus (); 

  struct FieldInfo {
    friend class FarmGraphicsInfo; 
    FieldInfo (FieldShape s); 
    int getIndex () const {return textureIndices[status];} 
    
    cpit begin () const {return shape.begin();}
    pit  begin ()       {return shape.begin();}
    cpit end () const {return shape.end();}
    pit  end ()       {return shape.end();}
    
  private:
    FieldShape shape;
    double area;
    int status;
  };

  int getHouses () const;
  
  typedef vector<FieldInfo>::const_iterator cfit;
  typedef vector<FieldInfo>::iterator fit;
  cfit start () const {return fields.begin();}
  cfit final () const {return fields.end();} 
  fit start () {return fields.begin();}
  fit final () {return fields.end();} 
  cpit startDrill () const {return exercis.begin();}
  cpit finalDrill () const {return exercis.end();}
  cpit startHouse () const {return village.begin();}
  cpit finalHouse () const {return village.end();}
  cpit startSheep () const {return pasture.begin();}
  cpit finalSheep () const {return pasture.end();}
  
  typedef vector<FarmGraphicsInfo*>::iterator Iterator;
  static Iterator begin () {return allFarmInfos.begin();}
  static Iterator end () {return allFarmInfos.end();}  
  
  
private:  
  double fieldArea ();   
  void generateShapes (HexGraphicsInfo* dat);
  vector<FieldInfo> fields;
  FieldShape exercis;
  FieldShape village;
  FieldShape pasture; 
  static vector<int> textureIndices; 
  
  Farmland* myFarm;
  static vector<FarmGraphicsInfo*> allFarmInfos; 
};

class HexGraphicsInfo : public GraphicsInfo {
public:
  HexGraphicsInfo (Hex* h); 
  ~HexGraphicsInfo ();

  typedef vector<HexGraphicsInfo*>::const_iterator Iterator;
  
  virtual void describe (QTextStream& str) const;  
  triplet getCoords (Vertices v) const;
  FarmGraphicsInfo const* getFarm () const {return farmInfo;} 
  Hex* getHex () const {return myHex;}
  FieldShape getPatch (bool large = false); 
  TerrainType getTerrain () const {return terrain;}
  bool isInside (double x, double y) const;
  void setFarm (FarmGraphicsInfo* f);
  static Iterator begin () {return allHexGraphics.begin();}
  static Iterator end   () {return allHexGraphics.end();}
  static void getHeights (); 

  cpit startTrees () const {return trees.begin();}
  cpit finalTrees () const {return trees.end();}
  
private:
  void generateShapes ();
  double patchArea () const;
  
  TerrainType terrain;
  Hex* myHex;
  triplet cornerRight;
  triplet cornerRightDown;
  triplet cornerLeftDown;
  triplet cornerLeft;
  triplet cornerLeftUp;
  triplet cornerRightUp;  
  FarmGraphicsInfo* farmInfo;
  vector<FieldShape> spritePatches; // Places to put sprites - hand out to subordinates.
  vector<FieldShape> biggerPatches; // For larger shapes like drill grounds.   
  FieldShape trees; // Not a polygon.
  vector<int> treesPerField;
  
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


class MilUnitGraphicsInfo : public GraphicsInfo {
  friend class StaticInitialiser;
public:
  MilUnitGraphicsInfo (MilUnit* dat) : myUnit(dat) {}  
  ~MilUnitGraphicsInfo ();

  struct spriterator {
    spriterator (int idx, MilUnitGraphicsInfo const* const b) : index(idx), boss(b) {}
    ThreeDSprite* operator* () {if ((int) boss->spriteIndices.size() <= index) return sprites[0]; return sprites[boss->spriteIndices[index]];}
    void operator++ () {index++;}
    bool operator!= (const spriterator& other) {return index != other.index;}
    bool operator== (const spriterator& other) {return index == other.index;}    

  private:
    int index;
    MilUnitGraphicsInfo const* const boss; 
  };
  
  spriterator start () const {return spriterator(0, this);}
  spriterator final () const {return spriterator(spriteIndices.size(), this);}

  virtual void describe (QTextStream& str) const;
  string strengthString (string indent) const;  
  void updateSprites (MilStrength* dat); 
  
private:
  MilUnit* myUnit;
  vector<int> spriteIndices; 
  static vector<ThreeDSprite*> sprites;
  static map<MilUnitTemplate*, int> indexMap; 
};

#endif
