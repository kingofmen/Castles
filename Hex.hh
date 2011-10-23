#ifndef HEX_HH
#define HEX_HH

#include <vector>
#include <set> 
#include <map> 
#include <QString>
#include <string>
#include "Mirrorable.hh"
#include "Building.hh" 

class PopUnit;
class MilUnit;
class Player; 
class Vertex; 
class Line;

class Named {
public:
  std::string getName() const {return name;}
  void setName (std::string n) {name = n;} 
private:
  std::string name;
};

class Hex : public Mirrorable<Hex>, public Named {
public:
  enum TerrainType {Mountain = 0, Hill, Plain, Forest, Ocean, Unknown}; 
  enum Direction {NorthWest = 0, North, NorthEast, SouthEast, South, SouthWest, None};
  enum Vertices {LeftUp = 0, RightUp, Right, RightDown, LeftDown, Left, NoVertex}; 
  
  Hex (int x, int y, TerrainType t);
  ~Hex ();

  typedef std::vector<Line*>::iterator LineIterator; 
  typedef std::vector<PopUnit*>::iterator PopIterator;
  typedef std::vector<Vertex*>::iterator VtxIterator; 
  typedef std::set<Hex*>::iterator Iterator; 

  void raid () {devastation++;}
  void repair () {if (0 < devastation) devastation--;} 
  int getDevastation () const {return devastation;} 
  void setNeighbour (Direction d, Hex* dat);
  void createVertices (); 
  int numPops () const {return units.size();}
  PopUnit* removePop () {PopUnit* ret = units.back(); units.pop_back(); return ret;}
  MilUnit* mobilise (); 
  void addPop (PopUnit* p) {units.push_back(p);} 
  std::pair<int, int> getPos () const {return pos;}
  std::pair<int, int> getPos (Direction dat) const; 
  TerrainType getType () const {return myType;}
  int numMovesTo (Hex const * const dat) const;
  void endOfTurn ();
  void production (); 
  void setOwner (Player* p);
  Player* getOwner () {return owner;} 
  Vertex* getVertex (int i);
  Line* getLine (Direction dir) {return lines[dir];}
  Hex::Vertices getDirection (Vertex const * const ofdis) const;
  Hex::Direction getDirection (Hex const * const dat) const;
  Hex::Direction getDirection (Line const * const dat) const; 
  std::string toString () const;
  VtxIterator vexBegin () {return vertices.begin();}
  VtxIterator vexEnd   () {return vertices.end();} 
  LineIterator linBegin () {return lines.begin();}
  LineIterator linEnd   () {return lines.end();} 
  void setLine (Hex::Direction dir, Line* l); 
  virtual void setMirrorState (); 
  
  static Iterator begin () {return allHexes.begin();}
  static Iterator end () {return allHexes.end();}  
  static TerrainType getType (char t); 
  static std::string getDirectionName (Direction dat);
  static std::string getVertexName (Vertices dat);
  static std::pair<int, int> getNeighbourCoordinates (std::pair<int, int> pos, Direction dere);
  static Direction getDirection (std::string n);
  static Vertices getVertex (std::string n);
  static Direction convertToDirection (int n);
  static Vertices convertToVertex (int i);
  static Direction oppositeDirection (Direction dat);
  static Vertices oppositeVertex (Vertices dat);
  static void clear (); 
private:
  unsigned int maxPopulation () const; 

  std::vector<Line*> lines; 
  std::vector<Vertex*> vertices;
  std::vector<Hex*> neighbours;
  std::vector<PopUnit*> units; 
  std::pair<int, int> pos;
  double popGrowth; 
  TerrainType myType;
  Player* owner;
  int devastation; 
  
  static std::set<Hex*> allHexes;
};

class Vertex : public Mirrorable<Vertex>, public Named {
  friend class Hex; 
public:
  Vertex();
  ~Vertex();

  typedef std::set<Vertex*>::iterator Iterator;
  typedef std::vector<Vertex*>::iterator NeighbourIterator;   
  typedef std::vector<Hex*>::iterator HexIterator; 
  
  MilUnit* removeUnit () {MilUnit* ret = units.back(); units.pop_back(); return ret;}
  void addUnit (MilUnit* dat); 
  int numUnits () const {return units.size();}
  MilUnit* getUnit (int i) {return units[i];}
  Line* getLine (Vertex* otherend); 
  Hex::Vertices getDirection (Vertex const * const dat) const;
  Vertex* getNeighbour (int i) {return neighbours[i];} 
  void endOfTurn ();
  void addSupplies (double s);
  bool canTakeSupplies (Player* p) const;
  QString toString ();
  NeighbourIterator beginNeighbours () {return neighbours.begin();}
  Hex::LineIterator beginLines () {return lines.begin();}
  HexIterator       beginHexes () {return hexes.begin();}
  NeighbourIterator endNeighbours () {return neighbours.end();}
  Hex::LineIterator endLines () {return lines.end();}
  HexIterator       endHexes () {return hexes.end();}

  double deliveredTo (Vertex* dat) {return delivery[dat];} 
  double getDefenseModifier () const;
  double supplyNeeded () const; 
  int roadLevel (Vertex* dere) {return roads[dere];} 
  void buildRoad (Vertex* target);
  void buildCastle (); 
  bool isLand () const; 
  Hex* getHex (int i) {return hexes[i];} 
  void createLines (); 
  void forceRetreat (Castle*& c, Vertex*& v); 
  virtual void setMirrorState ();
  
  static void logistics (Player* p);
  static void clearForLogistics ();
  static Iterator begin () {return allVertices.begin();}
  static Iterator end () {return allVertices.end();} 
  static void clear (); 
  
private:
  double resistance (Player* p, Vertex* n);
  double potential (Player* p);
  void makeFlow (Player* p);
  void reconcile ();
  double deliverSupplies (Player* p); 
  void seedGroup (Player* p); 
  
  std::map<Vertex*, double> flow;
  std::map<Vertex*, double> tempFlow;
  std::map<Vertex*, double> delivery; 
  std::vector<Vertex*> neighbours;
  std::map<Vertex*, int> roads; 
  std::vector<Hex*> hexes;
  std::vector<Line*> lines; 
  std::vector<MilUnit*> units;
  double supplies; 
  int groupNum;
  int fortLevel; 
  
  static std::set<Vertex*> allVertices;
};

class Line : public Mirrorable<Line>, public Named {
public:
  Line (Vertex* one, Vertex* two, Hex* hone, Hex* thwo); 
  ~Line (); 

  Vertex* getOther (Vertex* vex); 
  Vertex* oneEnd () const {return vex1;}
  Vertex* twoEnd () const {return vex2;}
  Vertex* getVtx (int idx) {return (idx == 0 ? vex1 : vex2);} 
  Hex* oneHex () {return hex1;}
  Hex* twoHex () {return hex2;}
  Hex* otherHex (Hex* dat); 
  void addCastle (Castle* dat); 
  Castle* getCastle () {return castle;}
  virtual void setMirrorState ();
  
  typedef std::set<Line*>::iterator Iterator;
  static Iterator begin () {return allLines.begin();}
  static Iterator end () {return allLines.end();} 
  static void clear (); 
  
private:
  Vertex* vex1;
  Vertex* vex2;
  Hex* hex1;
  Hex* hex2;
  Castle* castle;

  static std::set<Line*> allLines;
};

#endif
