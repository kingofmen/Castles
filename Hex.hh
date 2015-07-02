#ifndef HEX_HH
#define HEX_HH

#include <vector>
#include <set>
#include <map>
#include <QString>
#include <string>
#include "Mirrorable.hh"
#include "Building.hh"
#include "GraphicsInfo.hh"
#include "UtilityFunctions.hh"
#include "Directions.hh"

class MilUnit;
class MilUnitTemplate;
class Player;
class Vertex;
class Line;

class Hex : public Mirrorable<Hex>, public Named<Hex>, public Iterable<Hex> {
  friend class Mirrorable<Hex>;
  friend class StaticInitialiser;
public:
  ~Hex ();

  typedef vector<Line*>::iterator LineIterator;
  typedef vector<Vertex*>::iterator VtxIterator;

  bool                   colonise (Line* lin, MilUnit* unit, Outcome out);
  void                   createVertices ();
  void                   endOfTurn ();
  Castle*                getCastle ();
  Vertices               getDirection (Vertex const * const ofdis) const;
  Direction              getDirection (Hex const * const dat) const;
  Direction              getDirection (Line const * const dat) const;
  Farmland*              getFarm () {return farms;}
  Forest*                getForest () {return forest;}
  HexGraphicsInfo const* getGraphicsInfo () const {return graphicsInfo;}
  Line*                  getLine (Direction dir) {return lines[dir];}
  Market*                getMarket () const;
  Mine*                  getMine () {return mine;}
  Player*                getOwner () {return owner;}
  pair<int, int>         getPos () const {return pos;}
  pair<int, int>         getPos (Direction dat) const;
  int                    getTotalPopulation () const;
  TerrainType            getType () const {return myType;}
  Vertex*                getVertex (int i);
  Village*               getVillage () {return village;}
  LineIterator           linBegin () {return lines.begin();}
  LineIterator           linEnd   () {return lines.end();}
  GoodsHolder            loot (double lootRatio);
  int                    numMovesTo (Hex const * const dat) const;
  void                   raid (MilUnit* raiders, Outcome out);
  void                   repair (Outcome out);
  void                   setNeighbour (Direction d, Hex* dat);
  void                   setOwner (Player* p);
  string                 toString () const;
  VtxIterator            vexBegin () {return vertices.begin();}
  VtxIterator            vexEnd   () {return vertices.end();}
  int                    recruit (Player* forhim, MilUnitTemplate const* const recruitType, MilUnit* target, Outcome out);
  void                   setLine (Direction dir, Line* l);
  void                   setFarm (Farmland* f);
  void                   setForest (Forest* f);  
  void                   setGraphicsFarm (Farmland* f);
  void                   setGraphicsVillage (Village* v);
  void                   setMine (Mine* m);  
  void                   setVillage (Village* v);
  virtual void           setMirrorState ();

  static TerrainType getType (char t);
  static pair<int, int> getNeighbourCoordinates (pair<int, int> pos, Direction dere);
  static Hex* getHex (int x, int y);
  static Hex* getTestHex (bool vi = true, bool fa = true, bool fo = true, bool mi = true);
  static void clear ();
  static void createHex (int x, int y, TerrainType t);
  static void unitTests ();

private:
  Hex (int x, int y, TerrainType t);
  Hex (Hex* other);
  void initialise ();

  vector<Line*> lines;
  vector<Vertex*> vertices;
  vector<Hex*> neighbours;
  pair<int, int> pos;
  TerrainType myType;
  Player* owner;
  HexGraphicsInfo* graphicsInfo;
  Farmland* farms;
  Forest* forest;
  Mine* mine;
  Village* village;
  Castle* castle;
  int arableLand;
  Vertex* marketVtx;
};

class Vertex : public Mirrorable<Vertex>, public Named<Vertex>, public Iterable<Vertex> {
  friend class Mirrorable<Vertex>;
  friend class StaticInitialiser;
  friend class Hex;
public:
  Vertex();
  ~Vertex();

  typedef vector<Vertex*>::iterator NeighbourIterator;
  typedef vector<Hex*>::iterator HexIterator;
  typedef vector<MilUnit*>::iterator UnitIterator;

  MilUnit* removeUnit () {MilUnit* ret = units.back(); units.pop_back(); return ret;}
  void addUnit (MilUnit* dat);
  int numUnits () const {return units.size();}
  MilUnit* getUnit (int i) {if (i >= (int) units.size()) return 0; if (i < 0) return 0; return units[i];}
  Line* getLine (Vertex* otherend);
  Vertices getDirection (Vertex const * const dat) const;
  Market* getMarket () const {return theMarket;}
  Vertex* getNeighbour (int i) {return neighbours[i];}
  void endOfTurn ();
  QString toString ();
  NeighbourIterator beginNeighbours () {return neighbours.begin();}
  Hex::LineIterator beginLines () {return lines.begin();}
  HexIterator       beginHexes () {return hexes.begin();}
  UnitIterator      beginUnits () {return units.begin();}
  NeighbourIterator endNeighbours () {return neighbours.end();}
  Hex::LineIterator endLines () {return lines.end();}
  HexIterator       endHexes () {return hexes.end();}
  UnitIterator      endUnits () {return units.end();}

  VertexGraphicsInfo const* getGraphicsInfo () const {return graphicsInfo;}
  double traversalCost (Vertex* dat) const;

  struct DistanceHeuristic {
    virtual double operator ()(Vertex* dat) const = 0;
  };

  struct NoHeuristic : public DistanceHeuristic {
    virtual double operator ()(Vertex* dat) const {return 0;}
  };

  struct VertexDistance : public DistanceHeuristic {
    VertexDistance (Vertex const* t) : target(t) {}
    virtual double operator ()(Vertex* dat) const {return target->position.distance(dat->position);}
    Vertex const* target;
  };

  struct CastleDistance : public DistanceHeuristic {
    CastleDistance (Castle const* c) : target(c) {}
    virtual double operator ()(Vertex* dat) const;
    Castle const* target;
  };

  struct GoalChecker {
    virtual bool operator ()(Vertex* dat) const = 0;
  };

  struct VertexEquality : public GoalChecker {
    VertexEquality (Vertex const* t) : target(t) {}
    virtual bool operator ()(Vertex* dat) const {return dat == target;}
    Vertex const* target;
  };

  struct CastleFinder : public GoalChecker {
    CastleFinder (Castle const* c) : target(c) {}
    virtual bool operator ()(Vertex* dat) const;
    Castle const* target;
  };

  void setMarket (Market* tm) {theMarket = tm;}
  double supplyNeeded () const;
  bool isLand () const;
  void findRoute (vector<Vertex*>& vertices, const GoalChecker& gc, const DistanceHeuristic& heuristic);
  void findRouteToVertex (vector<Vertex*>& vertices, Vertex const* targetVertex);
  Hex* getHex (int i) {return hexes[i];}
  void createLines ();
  void forceRetreat (Castle*& c, Vertex*& v);
  virtual void setMirrorState ();

  static void clear ();

private:
  Vertex (Vertex* other);

  vector<Vertex*> neighbours;
  vector<Hex*> hexes;
  vector<Line*> lines;
  vector<MilUnit*> units;
  int groupNum;
  VertexGraphicsInfo* graphicsInfo;
  doublet position;
  Market* theMarket;
};

class Line : public Mirrorable<Line>, public Named<Line>, public Iterable<Line> {
  friend class Mirrorable<Line>;
  friend class StaticInitialiser;
public:
  Line (Vertex* one, Vertex* two, Hex* hone, Hex* thwo);
  ~Line ();

  void endOfTurn ();
  Vertex* getOther (Vertex* vex);
  Vertex* oneEnd () const {return vex1;}
  Vertex* twoEnd () const {return vex2;}
  Vertex* getVtx (int idx) {return (idx == 0 ? vex1 : vex2);}
  doublet getPosition () const {return position;}
  Hex* oneHex () {return hex1;}
  Hex* twoHex () {return hex2;}
  Hex* otherHex (Hex* dat);
  void addCastle (Castle* dat);
  Castle* getCastle () {return castle;}
  LineGraphicsInfo const* getGraphicsInfo () const {return graphicsInfo;}
  virtual void setMirrorState ();

  static void clear ();

private:
  Line (Line* other);

  Vertex* vex1;
  Vertex* vex2;
  Hex* hex1;
  Hex* hex2;
  Castle* castle;
  LineGraphicsInfo* graphicsInfo;
  doublet position;
};

#endif
