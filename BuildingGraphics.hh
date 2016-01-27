#ifndef BUILDING_GRAPHICS_HH
#define BUILDING_GRAPHICS_HH

#include "GraphicsBridge.hh"
#include "GraphicsInfo.hh"

class Farmland;
class FieldStatus;
class Castle;
class Market;
class Village;
class HexGraphicsInfo;

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

class CastleGraphicsInfo : public GraphicsInfo, public TextInfo, public GBRIDGE(Castle) {
public:
  CastleGraphicsInfo (Castle* castle);
  ~CastleGraphicsInfo ();

  double getAngle () const {return angle;}
private:
  double angle;
};

class MarketGraphicsInfo : public TBRIDGE(Market) {
public:
  MarketGraphicsInfo (Market* dat);
  ~MarketGraphicsInfo ();
};

#endif
