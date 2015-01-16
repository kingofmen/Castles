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

class PopUnit;
class MilUnit;
class MilUnitTemplate;
class Player;
class Vertex;
class Line;

class Hex : public Mirrorable<Hex>, public Named<Hex>, public Iterable<Hex>, public Market {
  friend class Mirrorable<Hex>;
  friend class StaticInitialiser;
public:
  ~Hex ();

  typedef vector<Line*>::iterator LineIterator;
  typedef vector<PopUnit*>::iterator PopIterator;
  typedef vector<Vertex*>::iterator VtxIterator;

  void                   addPop (PopUnit* p) {units.push_back(p);}
  bool                   colonise (Line* lin, MilUnit* unit, Outcome out);
  void                   createVertices ();
  void                   endOfTurn ();
  Vertices               getDirection (Vertex const * const ofdis) const;
  Direction              getDirection (Hex const * const dat) const;
  Direction              getDirection (Line const * const dat) const;
  Farmland*              getFarm () {return farms;}
  HexGraphicsInfo const* getGraphicsInfo () const {return graphicsInfo;}
  Line*                  getLine (Direction dir) {return lines[dir];}
  Player*                getOwner () {return owner;}
  pair<int, int>         getPos () const {return pos;}
  pair<int, int>         getPos (Direction dat) const;
  int                    getTotalPopulation () const;
  TerrainType            getType () const {return myType;}
  Vertex*                getVertex (int i);
  Village*               getVillage () {return village;}
  LineIterator           linBegin () {return lines.begin();}
  LineIterator           linEnd   () {return lines.end();}
  int                    numMovesTo (Hex const * const dat) const;
  int                    numPops () const {return units.size();}
  void                   raid (MilUnit* raiders, Outcome out);
  PopUnit*               removePop () {PopUnit* ret = units.back(); units.pop_back(); return ret;}
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
  vector<PopUnit*> units;
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
};

class Geography {
public:
  Geography () {}
  virtual ~Geography ();
  virtual void getNeighbours (vector<Geography*>& ret) = 0;
  double heuristicDistance (Geography* other) const;
  virtual double traversalCost (Player* side) const = 0;
  virtual double traversalRisk (Player* side) const = 0;
  virtual double traverseSupplies (double& amount, Player* side, Geography* previous);
  void clearGeography ();

  pair<double, double> position;
  double distFromStart;
  Geography* previous;
  bool closed;
};


class Vertex : public Mirrorable<Vertex>, public Named<Vertex>, public Geography, public Iterable<Vertex> {
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
  virtual void getNeighbours (vector<Geography*>& ret);
  virtual double traversalCost (Player* side) const;
  virtual double traversalRisk (Player* side) const;

  double supplyNeeded () const;
  bool isLand () const;
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
};

class Line : public Mirrorable<Line>, public Named<Line>, public Geography, public Iterable<Line> {
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
  Hex* oneHex () {return hex1;}
  Hex* twoHex () {return hex2;}
  Hex* otherHex (Hex* dat);
  void addCastle (Castle* dat);
  void addGraphicCastle (Castle* dat);
  Castle* getCastle () {return castle;}
  LineGraphicsInfo const* getGraphicsInfo () const {return graphicsInfo;}
  virtual void setMirrorState ();

  virtual void getNeighbours (vector<Geography*>& ret);
  virtual double traversalCost (Player* side) const;
  virtual double traversalRisk (Player* side) const;
  virtual double traverseSupplies (double& amount, Player* side, Geography* previous);
  static void clear ();

private:
  Line (Line* other);

  Vertex* vex1;
  Vertex* vex2;
  Hex* hex1;
  Hex* hex2;
  Castle* castle;
  LineGraphicsInfo* graphicsInfo;
};

#endif
