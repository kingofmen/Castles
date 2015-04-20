#ifndef MILUNIT_HH
#define MILUNIT_HH

#include <vector>
#include <stack> 
#include "PopUnit.hh"
#include "Logger.hh" 
#include "AgeTracker.hh"
#include "UtilityFunctions.hh"
#include "EconActor.hh" 


class Vertex; 
struct BattleResult; 

class MilUnitTemplate {
  friend class StaticInitialiser; 
public:
  MilUnitTemplate (string n);
  ~MilUnitTemplate (); 
  
  string name;
  double base_shock;
  double base_range;
  double base_defense;
  double base_tacmob;  
  //map<string, int> synergies;
  double supplyConsumption;
  int recruit_speed; 
  double militiaDecay;
  double militiaDrill; // Amount of labour required to do one level of drill for one of these units.  
  
  typedef vector<string>::const_iterator TypeNameIterator;
  static TypeNameIterator beginTypeNames () {return allUnitTypeNames.begin();}
  static TypeNameIterator endTypeNames () {return allUnitTypeNames.end();}
  typedef set<MilUnitTemplate const*>::const_iterator Iterator;
  static Iterator begin () {return allUnitTypes.begin();}
  static Iterator end () {return allUnitTypes.end();} 

  static MilUnitTemplate const* getUnitType (string n) {return allUnitTemplates[n];} 
  static double getDrillEffect (int level) {if (level < 0) return drillEffects[0]; if (level >= (int) drillEffects.size()) return drillEffects.back(); return drillEffects[level];}
  static int getMaxDrill () {return drillEffects.size() - 1;} 
  
private: 
  
  static map<string, MilUnitTemplate const*> allUnitTemplates;
  static vector<string> allUnitTypeNames;
  static set<MilUnitTemplate const*> allUnitTypes;
  static vector<double> drillEffects; // Effects of drill levels on decay constant. 
};

class MilUnitElement : public Mirrorable<MilUnitElement> {
  friend class Mirrorable<MilUnitElement>;
public:
  MilUnitElement ();
  MilUnitElement (MilUnitElement* other);
  ~MilUnitElement ();  
  
  double shock;
  double range;
  double defense;
  double tacmob;  
  int strength () {return soldiers->getTotalPopulation();}
  virtual void setMirrorState ();
  
  MilUnitTemplate const * unitType;
  AgeTracker* soldiers; 
};

class MilUnit : public Unit, public EconActor, public Mirrorable<MilUnit>, public Named<MilUnit, false>, public MilStrength, public Iterable<MilUnit> {
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
  void battleCasualties (MilUnit* const adversary);
  double calcBattleCasualties (MilUnit* const adversary, BattleResult* outcome = 0);
  double calcRoutCasualties (MilUnit* const adversary);
  double effectiveMobility (MilUnit* const versus);
  double getDecayConstant () const {return defaultDecayConstant * (modStack.size() > 0 ? modStack.top() : 1);}
  int getFightingModifier (MilUnit* const adversary);
  int getScoutingModifier (MilUnit* const adversary);
  int getSkirmishModifier (MilUnit* const adversary);
  void routCasualties (MilUnit* const adversary);
  void setExtMod (double ext);
  void setFightingFraction (double frac = 1.0) {fightFraction = frac;} 
  void dropExtMod () {modStack.pop();} 
  void setAggression (double a) {aggression = max(a, min(a, 0.01));} 
  
  int totalSoldiers () const; 
  double calcStrength (double decayConstant, double MilUnitElement::*field);
  void setLocation (Vertex* dat) {location = dat;} 
  virtual void setMirrorState ();
  void setRear (Vertices r) {rear = r;}

  Vertex* getLocation () {return location;} 
  Vertices getRear () const {return rear;} 

  MilUnitElement* getElement (MilUnitTemplate const* ut);
  MilUnitGraphicsInfo const* getGraphicsInfo () {return graphicsInfo;}  
  double getSupplyRatio () const {return supplyRatio;} 
  double getPriority () const {return priorityLevels[priority];} 
  virtual int getUnitTypeAmount (MilUnitTemplate const* const ut) const; 
  double efficiency () const {return max(0.1, supplyRatio);} 
  void endOfTurn ();
  void incPriority (bool up = true) {setPriority(priority + (up ? 1 : -1));}
  void setPriority (int p) {if (p < 0) priority = 0; else if (p >= (int) priorityLevels.size()) priority = priorityLevels.size() - 1; else priority = p;}
  
  static void setPriorityLevels (vector<double> newPs);
  
private:
  MilUnit (MilUnit* other); 
  
  void recalcElementAttributes (); 
  int takeCasualties (double rate);
  void getShockRange (double shkRatio, double firRatio, double mobRatio, double& shkPercent, double& firPercent) const;
  
  Vertex* location; 
  Vertices rear;
  vector<MilUnitElement*> forces;
  double supplyRatio; 
  int priority;
  std::stack<double> modStack;
  double fightFraction;  // For use in hypotheticals: If this unit were at X% strength. 
  MilUnitGraphicsInfo* graphicsInfo; 
  double aggression;
  
  static vector<double> priorityLevels;
  static double defaultDecayConstant; 
}; 

struct CombatInfo {
  int casualties;
  double mobRatio;
  double shock;
  double range;
  double lossRate;
  double efficiency;
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

void battleReport (Logger& log, BattleResult& outcome); 

#endif
