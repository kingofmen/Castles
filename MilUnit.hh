#ifndef MILUNIT_HH
#define MILUNIT_HH

#include <list>
#include <stack> 
#include <vector>
#include "Logger.hh" 
#include "AgeTracker.hh"
#include "Directions.hh"
#include "EconActor.hh"
#include "Hex.hh"
#include "Mirrorable.hh"
#include "UtilityFunctions.hh"

class Castle;
class Vertex; 
struct BattleResult;
class TransportUnit;

struct SupplyLevel : public GoodsHolder {
  SupplyLevel (string n) : name(n), desertionModifier(1.0), fightingModifier(0.01), movementModifier(0.1) {}
  string name;
  double desertionModifier;
  double fightingModifier;
  double movementModifier;
};

typedef vector<SupplyLevel>::const_iterator supIter;

class MilUnitTemplate : public Enumerable<MilUnitTemplate> {
  friend class StaticInitialiser; 
public:
  MilUnitTemplate (string n);
  ~MilUnitTemplate () {}

  double base_shock;
  double base_range;
  double base_defense;
  double base_tacmob;  
  //map<string, int> synergies;
  vector<SupplyLevel> supplyLevels;
  int recruit_speed; 
  double militiaDecay;
  double militiaDrill; // Amount of labour required to do one level of drill for one of these units.  

  static double getDrillEffect (int level) {if (level < 0) return drillEffects[0]; if (level >= (int) drillEffects.size()) return drillEffects.back(); return drillEffects[level];}
  static int getMaxDrill () {return drillEffects.size() - 1;} 

private:
  static vector<double> drillEffects; // Effects of drill levels on decay constant.
};

class MilUnitElement : public Mirrorable<MilUnitElement> {
  friend class Mirrorable<MilUnitElement>;
public:
  MilUnitElement (MilUnitTemplate const* const mut);
  MilUnitElement (MilUnitElement* other);
  ~MilUnitElement ();  
  
  double shock;
  double range;
  double defense;
  double tacmob;
  void reCalculate ();
  int strength () const {return soldiers->getTotalPopulation();}
  virtual void setMirrorState ();
  
  MilUnitTemplate const * unitType;
  AgeTracker* soldiers;
  vector<SupplyLevel>::const_iterator supply;
};

class Unit : public EconActor {
public: 
  Unit ();
  ~Unit() {}

  virtual void setLocation (Vertex* dat) {location = dat;}
  void setOwner (Player* p) {player = p;}
  void setRear (Vertices r) {rear = r;}
  Vertex* getLocation () const {return location;}
  Player* getOwner () const {return player;}
  Vertices getRear () const {return rear;}

protected:
  Vertex* location;
  Vertices rear;
private:
  Player* player;
};

class MilUnit : public Unit, public Mirrorable<MilUnit>, public Named<MilUnit, false>, public MilStrength, public Iterable<MilUnit> {
  friend class Mirrorable<MilUnit>;
  friend class StaticInitialiser;
  friend class MilUnitGraphicsInfo; 
public:
  MilUnit ();
  ~MilUnit ();

  // Strength manipulations
  void addElement (MilUnitTemplate const* const temp, AgeTracker& str);
  //void clear ();
  //MilUnit* detach (double fraction);
  void demobilise (AgeTracker& target); 
  //void transfer (MilUnit* target, double fraction); 

  // Fighting numbers:
  BattleResult attack (MilUnit* const adversary, Outcome dieroll = Neutral);  
  double calcBattleCasualties (MilUnit* const adversary, BattleResult* outcome = 0);
  double calcRoutCasualties (MilUnit* const adversary);
  double effectiveMobility (MilUnit* const versus);
  virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist);
  Castle const* getCastle () const {return castle;}
  double getDecayConstant () const {return defaultDecayConstant * (modStack.size() > 0 ? modStack.top() : 1);}
  double getForageStrength ();
  int getFightingModifier (MilUnit* const adversary);
  int getScoutingModifier (MilUnit* const adversary);
  int getSkirmishModifier (MilUnit* const adversary);
  void receiveTransportUnit (TransportUnit* transport);
  void setExtMod (double ext);
  void setFightingFraction (double frac = 1.0) {fightFraction = frac;} 
  void dropExtMod () {modStack.pop();} 
  void setAggression (double a) {aggression = min(1.0, max(a, 0.01));}
  void setCastle (Castle const* c) {castle = c;}
  virtual void setLocation (Vertex* dat);
  int totalSoldiers () const; 
  double calcStrength (double decayConstant, double MilUnitElement::*field);
  virtual void setMirrorState ();

  MilUnitElement* getElement (MilUnitTemplate const* ut);
  MilUnitGraphicsInfo const* getGraphicsInfo () {return graphicsInfo;}  
  double getPriority () const {return priorityLevels[priority];} 
  virtual int getUnitTypeAmount (MilUnitTemplate const* const ut) const; 
  void endOfTurn ();
  void incPriority (bool up = true) {setPriority(priority + (up ? 1 : -1));}
  void setPriority (int p) {if (p < 0) priority = 0; else if (p >= (int) priorityLevels.size()) priority = priorityLevels.size() - 1; else priority = p;}

  static MilUnitTemplate const* getTestType();
  static MilUnit* getTestUnit ();
  static void setPriorityLevels (vector<double> newPs);
  static void unitTests ();

private:
  MilUnit (MilUnit* other); 

  void consumeSupplies ();
  void forage ();
  void lootHex (Hex* hex);
  void recalcElementAttributes (); 
  int takeCasualties (double rate);
  void getShockRange (double shkRatio, double firRatio, double mobRatio, double& shkPercent, double& firPercent) const;

  vector<MilUnitElement*> forces;
  int priority;
  std::stack<double> modStack;
  double fightFraction;  // For use in hypotheticals: If this unit were at X% strength. 
  MilUnitGraphicsInfo* graphicsInfo; 
  double aggression;
  Castle const* castle;
  
  static vector<double> priorityLevels;
  static double defaultDecayConstant; 
}; 

struct CombatInfo {
  int casualties;
  double mobRatio;
  double shock;
  double range;
  double lossRate;
  double fightingFraction;
  double decayConstant; 
};

struct BattleResult {
  Outcome attackerSuccess;
  CombatInfo attackerInfo;
  CombatInfo defenderInfo;
  double shockPercent;
  double rangePercent;
  Outcome dieRoll;
};

class TransportUnit : public Unit, public Iterable<TransportUnit>, public Mirrorable<TransportUnit> {
  friend class StaticInitialiser;
  friend class Mirrorable<TransportUnit>;
public:
  TransportUnit (MilUnit* t);
  virtual ~TransportUnit ();

  void endOfTurn ();
  virtual void setMirrorState ();

  static void cleanUp ();

private:
  TransportUnit (TransportUnit* other);

  MilUnit* target;

  static vector<TransportUnit*> forDeletion;
};

class TradeUnit : public Unit, public Iterable<TradeUnit>, public Mirrorable<TradeUnit> {
  friend class StaticInitialiser;
  friend class Mirrorable<TradeUnit>;
public:
  TradeUnit ();
  virtual ~TradeUnit ();

  void endOfTurn ();
  virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist);
  virtual void setLocation (Vertex* dat);
  virtual void setMirrorState ();

  static void unitTests ();
private:
  TradeUnit (TradeUnit* other);

  void findTradeTarget ();

  struct MarketFinder : public Vertex::GoalChecker {
    MarketFinder (TradeUnit const* const t) : boss(t) {}
    virtual bool operator ()(Vertex* dat) const;
    TradeUnit const* const boss;
    vector<Vertex*> verboten;
  };

  GoodsHolder lastPricesPaid;
  Vertex* mostRecentMarket;
  Vertex* tradingTarget;
  TradeGood::Iter goodToBuy;
};

void battleReport (Logger& log, BattleResult& outcome); 

#endif
