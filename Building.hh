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

template<class T> class Industry : public EconActor {
  friend class StaticInitialiser;

public:
  Industry (T* ind, double md = 1) : EconActor(), industry(ind) {}
  
  virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {
    // Goal is to maximise profit. Calculate how much labor we need to get in the harvest;
    // if the price of the expected food is more, bid for enough labor to do this turn's work.
    // If there is money left over, bid for equipment to reduce the amount of labor needed
    // next turn, provided the net-present-value of the reduction is higher than the cost
    // of the machinery.

    double totalExpectedProduction = 0;
    double totalToBuy = 0 - getAmount(TradeGood::Labor);
    double marginalDecline = industry->getMarginFactor();
    double marginFactor = 1;
    for (int i = 0; i < industry->numBlocks(); ++i) {
      double laborNeeded = industry->getLabourForBlock(i);
      double expectedProduction = industry->outputOfBlock(i) * marginFactor * inverseProductionTime;
      // This is approximate - it assumes that the same amount of labour
      // will be needed in every step of the production cycle.
      if (prices.getAmount(TradeGood::Labor) * laborNeeded < prices.getAmount(output) * expectedProduction) {
	totalToBuy += laborNeeded;
	totalExpectedProduction += expectedProduction;
	marginFactor *= marginalDecline;
      }
      else {
	// This is where we no longer make a profit.
	// TODO: Query owner about whether to do it
	// anyway for "subsistence agriculture" or other
	// non-economic benefit.
	break;
      }
    }

    double neededForMaintenance = industry->labourForMaintenance();
    if (prices.getAmount(TradeGood::Labor) * neededForMaintenance < prices.getAmount(output) * industry->lossFromNoMaintenance()) {
      totalToBuy += neededForMaintenance;
    }
    
    if (0 < totalToBuy) bidlist.push_back(new MarketBid(TradeGood::Labor, totalToBuy, this));

    for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
      if (capital->getAmount(*tg) < 0.00001) continue;
      double laborSaving = totalToBuy * (1 - marginalCapFactor((*tg), getAmount(*tg)));
      // Assuming discount rate of 10%. Present value of amount x every period to infinity is (x/r) with r the interest rate.
      // TODO: Take decay into account. Variable discount rate?
      double npv = laborSaving * prices.getAmount(TradeGood::Labor) * 10;
      if (npv > prices.getAmount(*tg)) bidlist.push_back(new MarketBid((*tg), 1, this));
    }

    // Decide how much output to sell. On average, sell the inverse
    // of the length of the production cycle, thus keeping the output
    // constant. But sell more at higher prices. Expect that in the
    // long run we will sell at a 5% profit; make today's sale proportional
    // to the ratio of current profit to that expected five percent.

    if (1 > getAmount(output)) return;
    double expectedOutputPerLabour = totalExpectedProduction / (1 + totalToBuy);
    double profitPerLabour = expectedOutputPerLabour * prices.getAmount(output) - prices.getAmount(TradeGood::Labor);
    if (profitPerLabour < 0) return; // Don't sell at a loss.
    double profitPercentage = profitPerLabour / prices.getAmount(TradeGood::Labor);
    static const double inverseLongTermPercentage = 20; // Adjustable?
    profitPercentage *= inverseLongTermPercentage;
    double fractionToSell = min(1.0, inverseProductionTime * profitPercentage);
    fractionToSell *= getAmount(output);
    fractionToSell -= soldThisTurn.getAmount(output);
    if (1 > fractionToSell) return;
    bidlist.push_back(new MarketBid(output, -fractionToSell, this));
  }
  
  // Return the reduction in required labor if we had one additional unit.
  double marginalCapFactor (TradeGood const* const tg, double currentAmount) const {
    return capFactor(capital->getAmount(tg), 1+currentAmount) / capFactor(capital->getAmount(tg), currentAmount);
  }
  
  double capitalFactor (const GoodsHolder& capitalToUse, int dilution = 1) const {
    double ret = 1;
    for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
      ret *= capFactor(capital->getAmount(*tg), capitalToUse.getAmount(*tg)/dilution);
    }
    return ret;
  }
  
protected:
  virtual double getMarginFactor () const = 0;
  virtual double labourForMaintenance () const {return 0;}
  virtual double lossFromNoMaintenance () const {return 0;}
  virtual int numBlocks() const = 0;
  
  // Capital reduces the amount of labour required by factor (1 - x log (N+1)). This array stores x. 
  static GoodsHolder* capital;
  static TradeGood const* output;
  
private:
  T* industry;
  
  double capFactor (double reductionConstant, double goodAmount) const {
    return 1 - reductionConstant * log(1 + goodAmount);
  }

  static double inverseProductionTime;
};

template<class T> GoodsHolder* Industry<T>::capital = 0;
template<class T> TradeGood const* Industry<T>::output = 0;
template<class T> double Industry<T>::inverseProductionTime = 1;

class Building {
  friend class StaticInitialiser; 
public: 
  Building (double mf = 1) : marginFactor(mf), supplies(0), owner(0) {}
  ~Building () {}
  
  virtual void endOfTurn () = 0; 
  virtual void setOwner (Player* p); 
  Player* getOwner () {return owner;} 
  int getAssignedLand () const {return assignedLand;}
  void assignLand (int amount) {assignedLand += amount; if (0 > assignedLand) assignedLand = 0;}
  double getAvailableSupplies () const {return supplies;}   

  static const int numOwners = 10;  
protected:
  double marginFactor;
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

  static Village* getTestVillage (int pop);
  static void unitTests ();
  
protected: 
  AgeTracker males;
  AgeTracker women; 
  MilitiaTradition* milTrad;
  Farmland* farm; 

  struct MaslowLevel : public GoodsHolder {
    MaslowLevel () : GoodsHolder(), mortalityModifier(1.0), maxWorkFraction(1.0) {}
    MaslowLevel (double mm, double mwf) : GoodsHolder(), mortalityModifier(mm), maxWorkFraction(mwf) {}
    double mortalityModifier;
    double maxWorkFraction;
    string name;
  };

  MaslowLevel const* consumptionLevel;
  MaslowLevel const* expectedConsumptionLevel;
  
  static vector<MaslowLevel*> maslowLevels;
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
  string stopReason;
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

  static Farmland* getTestFarm (int numFields = 0);
  static void overrideConstantsForUnitTests (int lts, int ltp, int ltw, int ltr);
  static void unitTests ();
  
  static const int numOwners = 10; 
  
private:
  class Farmer : public Industry<Farmer>, public Mirrorable<Farmer> {
    friend class Mirrorable<Farmer>;
  public:
    Farmer (Farmland* b);
    ~Farmer ();
    double outputOfBlock (int b) const;
    double getLabourForBlock (int block) const;
    virtual int numBlocks () const;
    virtual double getMarginFactor () const {return boss->marginFactor;}
    virtual void setMirrorState ();
    void unitTests ();
    void workFields ();

    vector<int> fields;
  private:
    Farmer(Farmer* other);
    void fillBlock (int block, vector<int>& theBlock) const;
    Farmland* boss;
  };

  Farmland (Farmland* other);
  void countTotals ();
  int totalFields[NumStatus]; // Sum over farmers.
  vector<Farmer*> farmers;
  int blockSize;
  
  static int _labourToSow;
  static int _labourToPlow;
  static int _labourToClear;
  static int _labourToWeed;
  static int _labourToReap;
  static int _cropsFrom3;
  static int _cropsFrom2;
  static int _cropsFrom1;
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
  
private:
  class Forester : public Industry<Forester>, public Mirrorable<Forester> {
    friend class Mirrorable<Forester>;
  public:
    Forester (Forest* b);
    ~Forester ();
    double outputOfBlock (int b) const; 
    double getLabourForBlock (int block) const;
    virtual double getMarginFactor () const {return boss->marginFactor;}
    virtual double labourForMaintenance () const;
    virtual double lossFromNoMaintenance () const;
    virtual int numBlocks () const;
    virtual void setMirrorState ();
    void unitTests ();
    void workGroves (bool tick);

    vector<int> groves;
    int tendedGroves;
  private:
    Forester(Forester* other);
    Forest* boss;
    int    getForestArea () const;
    int    getTendedArea () const;
  };
 
  Forest (Forest* other);
  vector<Forester*> foresters;
  int yearsSinceLastTick;
  ForestStatus minStatusToHarvest;
  int blockSize;
  
  static vector<int> _amountOfWood;
  static int _labourToTend;    // Ensure forest doesn't go wild.
  static int _labourToHarvest; // Extract wood, make clear.
  static int _labourToClear;   // Go from wild to clear.
};

class Mine : public Building, public Mirrorable<Mine> {
  friend class StaticInitialiser;
  friend class Mirrorable<Mine>;
public:
  Mine ();
  ~Mine ();

  struct MineStatus : public Enumerable<MineStatus> {
    friend class StaticInitialiser;
    MineStatus (string n, int rl, bool lastOne = false);
    ~MineStatus ();
    int requiredLabour;
  };

  virtual void endOfTurn ();
  void setDefaultOwner (EconActor* o);
  void setMarket (Market* market) {BOOST_FOREACH(Miner* miner, miners) {market->registerParticipant(miner);}}
  virtual void setMirrorState ();

  static void unitTests ();
  
private:
  class Miner : public Industry<Miner>, public Mirrorable<Miner> {
    friend class Mirrorable<Miner>;
  public:
    Miner (Mine* m);
    ~Miner ();
    double outputOfBlock (int b) const; 
    double getLabourForBlock (int block) const;
    virtual double getMarginFactor () const {return mine->marginFactor;}
    virtual int numBlocks () const {return mine->veinsPerMiner;}
    virtual void setMirrorState ();
    void unitTests ();
    void workShafts ();

    vector<int> shafts;
  private:
    Miner(Miner* other);

    Mine* mine;
  };
 
  Mine (Mine* other);
  vector<Miner*> miners;
  int veinsPerMiner;
  static int _amountOfIron;
};

#endif
