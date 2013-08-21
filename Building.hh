#ifndef BUILDING_HH
#define BUILDING_HH

#include <vector> 
#include "Mirrorable.hh" 
#include "Logger.hh" 
#include "AgeTracker.hh" 
#include "UtilityFunctions.hh" 

class MilUnit;
class MilUnitTemplate;
class MilUnitGraphicsInfo;
class Hex;
class Line; 
class Player; 

struct ContractInfo {
  ContractInfo () : amount(0), delivery(Fixed) {}
  
  enum AmountType {Fixed, Percentage, SurplusPercentage};
  double amount;
  AmountType delivery;
  double delivered; 
};

class Building {
  friend class StaticInitialiser; 
public: 
  Building () {}
  ~Building () {}
  
  virtual void endOfTurn () = 0; 
  virtual void setOwner (Player* p) {owner = p;}
  Player* getOwner () {return owner;} 
  int getAssignedLand () const {return assignedLand;}
  void assignLand (int amount) {assignedLand += amount; if (0 > assignedLand) assignedLand = 0;}
  double getAvailableSupplies () const {return supplies;}   

protected:
  double supplies; 
  
private:
  Player* owner;
  int assignedLand; 
};

class Castle : public Building, public Mirrorable<Castle> {
  friend class Mirrorable<Castle>;
  friend class StaticInitialiser; 
public: 
  Castle (Hex* dat, Line* lin);
  ~Castle (); 

  void addGarrison (MilUnit* p);
  void callForSurrender (MilUnit* siegers, Outcome out); 
  virtual void endOfTurn ();
  Line* getLocation () {return location;}
  Hex* getSupport () {return support;}
  int numGarrison () const {return garrison.size();}
  MilUnit* getGarrison (unsigned int i) {if (i >= garrison.size()) return 0; return garrison[i];}
  void recruit (Outcome out);  
  MilUnit* removeGarrison ();
  double removeSupplies (double amount);   
  MilUnit* removeUnit (MilUnit* r);
  virtual void setOwner (Player* p);
  void setRecruitType (const MilUnitTemplate* m) {recruitType = m;}
  const MilUnitTemplate* getRecruitType () const {return recruitType;} 
  void supplyGarrison (); 
  virtual void setMirrorState ();
  
  static const int maxGarrison; 
  static double getSiegeMod () {return siegeModifier;} 
  
private:
  Castle (Castle* other);   
  
  vector<MilUnit*> garrison;
  Hex* support;
  Line* location; 
  const MilUnitTemplate* recruitType;
  ContractInfo taxExtraction; 
  
  static double siegeModifier; 
}; 

class MilitiaTradition : public Mirrorable<MilitiaTradition>, public MilStrength {
  friend class StaticInitialiser;
  friend class Mirrorable<MilitiaTradition>;
  friend class CivilBuilding; 
public:
  MilitiaTradition ();
  ~MilitiaTradition ();  
  
  void increaseTradition (MilUnitTemplate const* target = 0);
  void decayTradition ();
  double getRequiredWork (); 
  virtual void setMirrorState ();
  int getDrill () {return drillLevel;}
  int getStrength (MilUnitTemplate const* const dat) {return militiaStrength[dat];}
  void increaseDrill (bool up) {drillLevel += up ? 1 : -1;} 
  virtual int getUnitTypeAmount (MilUnitTemplate const* const ut) const;  // Slightly distinct from 'getStrength' in returning numbers of men, not regiments. 
  
private:
  MilitiaTradition (MilitiaTradition* other); 
  
  MilUnit* militia;
  map<MilUnitTemplate const* const, int> militiaStrength;
  int drillLevel;
};

class CivilBuilding : public Building { 
  friend class StaticInitialiser;
public:
  CivilBuilding ();
  ~CivilBuilding ();

  virtual void endOfTurn ();
  int getTotalPopulation () const {return males.getTotalPopulation() + women.getTotalPopulation();} 
  void demobMilitia (); 
  MilUnit* raiseMilitia (); 
  int produceRecruits (MilUnitTemplate const* const recruitType, MilUnit* target, Outcome dieroll);
  void increaseTradition (MilUnitTemplate const* target = 0) {milTrad->increaseTradition(target);} 
  MilitiaTradition* getMilitia () {return milTrad;} 
  const MilUnitGraphicsInfo* getMilitiaGraphics () const;
  
  int getMilitiaDrill () {return milTrad ? milTrad->getDrill() : 0;}
  int getMilitiaStrength (MilUnitTemplate const* const dat) {return milTrad ? milTrad->getStrength(dat) : 0;} 
  
protected:
  AgeTracker males;
  AgeTracker women; 
  MilitiaTradition* milTrad;
  double foodMortalityModifier; 
  //int turnsSincePopAdjust;   
  
  static vector<double> products;
  static vector<double> consume;
  static vector<double> recruitChance;   
  static double femaleProduction;
  static double femaleConsumption;
  static double femaleSurplusEffect; 
  static double femaleSurplusZero;
  
  
private:
  double adjustedMortality (int age, bool male) const;   

  static vector<double> baseMaleMortality;
  static vector<double> baseFemaleMortality;
  static vector<double> pairChance;
  static vector<double> fertility;
};

class Farmland : public CivilBuilding, public Mirrorable<Farmland> {
  friend class Mirrorable<Farmland>;
  friend class StaticInitialiser;
  friend class FarmGraphicsInfo; 
public:
  Farmland ();
  ~Farmland ();

  enum FieldStatus {Clear = 0, Ready, Sowed, Ripe1, Ripe2, Ripe3, Ended, NumStatus}; 
  
  double consumption () const;
  void devastate (int devastation);
  virtual void endOfTurn ();  
  double production () const;
  void workFields (); 
  virtual void setMirrorState ();
  void demandSupplies (ContractInfo* taxes);
  int getFieldStatus (int s) {return fields[s];}   
  int totalFields () const {return fields[Clear] + fields[Ready] + fields[Sowed] + fields[Ripe1] + fields[Ripe2] + fields[Ripe3] + fields[Ended];}
  
private:
  Farmland (Farmland* other);
  void eatFood (); 
  int fields[NumStatus]; 
  
  static int _labourToSow;
  static int _labourToPlow;
  static int _labourToClear;
  static int _labourToWeed;
  static int _labourToReap;
  static int _cropsFrom3;
  static int _cropsFrom2;
  static int _cropsFrom1; 
};


#endif
