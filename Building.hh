#ifndef BUILDING_HH
#define BUILDING_HH

#include <vector> 
#include "Market.hh"
#include "Mirrorable.hh" 
#include "Logger.hh" 
#include "AgeTracker.hh" 
#include "UtilityFunctions.hh" 
#include "EconActor.hh"

class MilUnit;
class MilUnitTemplate;
class MilUnitGraphicsInfo;
class Hex;
class Line; 
class Player;
class Farmland; 

template<class T> class Industry {
  friend class StaticInitialiser;

public:
  // Return the reduction in required labor if we had one additional unit.
  double marginalCapFactor (TradeGood const* const tg, double currentAmount) const {
    return capFactor(capital[*tg], 1+currentAmount) / capFactor(capital[*tg], currentAmount);
  }
  
  double capitalFactor (const GoodsHolder& capitalToUse, int dilution = 1) const {
    double ret = 1;
    for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
      ret *= capFactor(capital[**tg], capitalToUse.getAmount(*tg)/dilution);
    }
    return ret;
  }

protected:
  // Capital reduces the amount of labour required by factor (1 - x log (N+1)). This array stores x. 
  static double* capital;

private:
  double capFactor (double reductionConstant, double goodAmount) const {
    return 1 - reductionConstant * log(1 + goodAmount);
  }
}; 

template<class T> double* Industry<T>::capital; 

class Building {
  friend class StaticInitialiser; 
public: 
  Building () : owner(0) {}
  ~Building () {}
  
  virtual void endOfTurn () = 0; 
  virtual void setOwner (Player* p); 
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

class Castle : public Building, public EconActor, public Mirrorable<Castle> {
  friend class Mirrorable<Castle>;
  friend class StaticInitialiser; 
public: 
  Castle (Hex* dat, Line* lin);
  ~Castle (); 

  void addGarrison (MilUnit* p);
  void callForSurrender (MilUnit* siegers, Outcome out); 
  virtual void endOfTurn ();
  Line* getLocation () {return location;}
  MilUnit* getGarrison (unsigned int i) {if (i >= garrison.size()) return 0; return garrison[i];}
  Hex* getSupport () {return support;}
  int numGarrison () const {return garrison.size();}
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

protected:
  
private:
  Castle (Castle* other);   
  
  vector<MilUnit*> garrison;
  Hex* support;
  Line* location; 
  const MilUnitTemplate* recruitType;
  static double siegeModifier; 
}; 

class MilitiaTradition : public Mirrorable<MilitiaTradition>, public MilStrength {
  friend class StaticInitialiser;
  friend class Mirrorable<MilitiaTradition>;
  friend class Village; 
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

class Village : public Building, public EconActor, public Mirrorable<Village> { 
  friend class StaticInitialiser;
  friend class Mirrorable<Village>;
public:
  Village ();
  ~Village ();

  double consumption () const;
  void demobMilitia ();
  virtual void endOfTurn ();
  virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist);
  double getFractionOfMaxPop () const {double ret = getTotalPopulation(); ret /= maxPopulation; return min(1.0, ret);}
  MilitiaTradition* getMilitia () {return milTrad;} 
  const MilUnitGraphicsInfo* getMilitiaGraphics () const; 
  int getTotalPopulation () const {return males.getTotalPopulation() + women.getTotalPopulation();}
  MilUnit* raiseMilitia ();
  virtual double produceForContract (TradeGood const* const tg, double amount);
  int produceRecruits (MilUnitTemplate const* const recruitType, MilUnit* target, Outcome dieroll);
  double production () const;
  virtual void setMirrorState ();  
  void increaseTradition (MilUnitTemplate const* target = 0) {milTrad->increaseTradition(target);} 
  
  int getMilitiaDrill () {return milTrad ? milTrad->getDrill() : 0;}
  int getMilitiaStrength (MilUnitTemplate const* const dat) {return milTrad ? milTrad->getStrength(dat) : 0;} 
  void updateMaxPop () const {maxPopulation = max(maxPopulation, getTotalPopulation());} 
  void setFarm (Farmland* f) {farm = f;} 

  static void unitTests ();
  
protected: 
  AgeTracker males;
  AgeTracker women; 
  MilitiaTradition* milTrad;
  double foodMortalityModifier; 
  Farmland* farm; 

  struct MaslowLevel : public GoodsHolder {
    MaslowLevel () : GoodsHolder(), mortalityModifier(1.0) {}
    double mortalityModifier;
  };
  
  static vector<MaslowLevel> maslowLevels;
  static vector<double> products;
  static vector<double> consume;
  static vector<double> recruitChance;   
  static double femaleProduction;
  static double femaleConsumption;
  static double femaleSurplusEffect; 
  static double femaleSurplusZero;
  
private:
  Village (Village* other); 
  
  double adjustedMortality (int age, bool male) const;   
  void eatFood ();

  double workedThisTurn;
  static int maxPopulation; 
  static vector<double> baseMaleMortality;
  static vector<double> baseFemaleMortality;
  static vector<double> pairChance;
  static vector<double> fertility;
};

class Farmland : public Building, public Mirrorable<Farmland> {
  friend class Mirrorable<Farmland>;
  friend class StaticInitialiser;
  friend class FarmGraphicsInfo; 
public:
  Farmland ();
  ~Farmland ();

  enum FieldStatus {Clear = 0, Ready, Sowed, Ripe1, Ripe2, Ripe3, Ended, NumStatus};

  void setMarket (Market* market) {BOOST_FOREACH(Farmer* farmer, farmers) {market->registerParticipant(farmer);}}
  void devastate (int devastation);
  virtual void endOfTurn ();  
  void setDefaultOwner (EconActor* o); 
  virtual void setMirrorState ();
  int getFieldStatus (int s) {return totalFields[s];}
  void delivery (EconActor* target, TradeGood const* const good, double amount);
  int getTotalFields () const {return
      totalFields[Clear] +
      totalFields[Ready] +
      totalFields[Sowed] +
      totalFields[Ripe1] +
      totalFields[Ripe2] +
      totalFields[Ripe3] +
      totalFields[Ended];}

  static void unitTests ();
  
  static const int numOwners = 10; 
  
private:
  class Farmer : public EconActor, public Industry<Farmland::Farmer>, public Mirrorable<Farmland::Farmer> {
    friend class Mirrorable<Farmland::Farmer>;
  public:
    Farmer ();
    ~Farmer ();
    virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist);
    virtual double produceForContract (TradeGood const* const tg, double amount);
    virtual void setMirrorState ();
    void unitTests ();
    void workFields ();

    vector<int> fields;
  private:
    Farmer(Farmer* other);
    double getNeededLabour () const;
    double expectedOutput () const;
  };

  Farmland (Farmland* other);
  void countTotals ();
  int totalFields[NumStatus]; // Sum over farmers.
  vector<Farmer*> farmers;
  
  static int _labourToSow;
  static int _labourToPlow;
  static int _labourToClear;
  static int _labourToWeed;
  static int _labourToReap;
  static int _cropsFrom3;
  static int _cropsFrom2;
  static int _cropsFrom1;
  static TradeGood const* output;
};

class Forest : public Building, public Mirrorable<Forest> {
  friend class StaticInitialiser;
  friend class Mirrorable<Forest>;
public:
  Forest ();
  ~Forest ();

  enum ForestStatus {Clear = 0, Planted, Scrub, Saplings, Young, Grown, Mature, Mighty, Huge, Climax, Wild, NumStatus};

  virtual void endOfTurn ();
  void setDefaultOwner (EconActor* o);
  void setMarket (Market* market) {BOOST_FOREACH(Forester* forester, foresters) {market->registerParticipant(forester);}}
  virtual void setMirrorState ();

  static void unitTests ();
  
  static const int numOwners = 10;
private:
  class Forester : public EconActor, public Industry<Forester>, public Mirrorable<Forester> {
    friend class Mirrorable<Forester>;
  public:
    Forester (Forest* b);
    ~Forester ();
    virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist);
    virtual double produceForContract (TradeGood const* const tg, double amount);
    virtual void setMirrorState ();
    void unitTests ();
    void workGroves (bool tick);

    vector<int> groves;
    int tendedGroves;
  private:
    Forester(Forester* other);
    Forest* boss;
    int    getForestArea () const;
    double getNeededLabour () const;
    int    getTendedArea () const;
    double expectedOutput () const;
  };
 
  Forest (Forest* other);
  vector<Forester*> foresters;
  int yearsSinceLastTick;
  ForestStatus minStatusToHarvest;
  
  static vector<int> _amountOfWood;
  static int _labourToTend;    // Ensure forest doesn't go wild.
  static int _labourToHarvest; // Extract wood, make clear.
  static int _labourToClear;   // Go from wild to clear.
  static TradeGood const* output;
};

#endif
