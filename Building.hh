#ifndef BUILDING_HH
#define BUILDING_HH

#include <deque>
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
class VillageGraphicsInfo;
class Hex;
class Line; 
class Player;
class Farmland;
class Forest;
class Mine;

struct jobInfo : public boost::tuple<double, int, int> { // Labour amount, times needed, turns to complete.
  jobInfo (double l, double c, double t) : boost::tuple<double, int, int>(l, c, t) {}
  double& labourPerChunk () {return boost::get<0>(*this);}
  int& numChunks () {return boost::get<1>(*this);}
  int& numTurns () {return boost::get<2>(*this);}

  double labourPerChunk () const {return boost::get<0>(*this);}
  int numChunks () const {return boost::get<1>(*this);}
  int numTurns () const {return boost::get<2>(*this);}

  double totalLabour () const {return labourPerChunk() * numChunks();}
};

void searchForMatch (vector<jobInfo>& jobs, double perChunk, int chunks, int time);
void searchForMatch (vector<jobInfo>& jobs, jobInfo job);

struct BlockInfo {
public:
  BlockInfo (double mf = 1.0, int bs = 1, int wb = 1) : marginFactor(mf), blockSize(bs), workableBlocks(wb) {}
  double marginFactor;
  int blockSize;
  int workableBlocks; // Number of blocks that can be worked on in one turn.
};

template <class T> class Industry : public EconActor {
  friend class StaticInitialiser;

public:
  Industry (T* ind, BlockInfo* bi) : EconActor(), blockInfo(bi), industry(ind) {}
  
  virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {
    // Goal is to maximise profit. Calculate how much labor we need to get in the harvest;
    // if the price of the expected food is more, bid for enough labor to do this turn's work.
    // If there is money left over, bid for equipment to reduce the amount of labor needed
    // next turn, provided the net-present-value of the reduction is higher than the cost
    // of the machinery.

    static const double inverseExpectedRatio = 0.2;

    double marginFactor = 1;
    double marginalLabourRatio = 0;
    double fullCycleLabour = 0;
    double totalExpectedProduction = 0;
    vector<jobInfo> jobs;
    int lastBlock = 0;
    for (; lastBlock < industry->numBlocks(); ++lastBlock) {
      vector<jobInfo> candidateJobs;
      industry->getLabourForBlock(lastBlock, candidateJobs, fullCycleLabour);
      double expectedProduction = industry->outputOfBlock(lastBlock) * marginFactor;
      totalExpectedProduction += expectedProduction;
      if (prices.getAmount(TradeGood::Labor) * fullCycleLabour < prices.getAmount(output) * expectedProduction) {
	marginFactor *= blockInfo->marginFactor;
	marginalLabourRatio = expectedProduction / fullCycleLabour;
	BOOST_FOREACH(jobInfo job, candidateJobs) searchForMatch(jobs, job);
      }
      else {
	// This is where we no longer make a profit.
	// TODO: Query owner about whether to do it
	// anyway for "subsistence agriculture" or other
	// non-economic benefit.
	break;
      }
    }

    double reserveLabour = getAmount(TradeGood::Labor);
    reserveLabour -= promisedToDeliver.getAmount(TradeGood::Labor); // Probably negative
    reserveLabour -= soldThisTurn.getAmount(TradeGood::Labor); // Probably zero
    double totalLabourUsed = 0;
    BOOST_FOREACH(jobInfo job, jobs) {
      double perChunk = job.labourPerChunk();
      int chunks      = job.numChunks();
      int turns       = job.numTurns();
      if (1 > turns) continue;
      double neededPerTurn = perChunk * chunks;
      totalLabourUsed += neededPerTurn;
      neededPerTurn /= turns;
      if (neededPerTurn < reserveLabour) {
	reserveLabour -= neededPerTurn;
	continue;
      }
      // Buy multiples of the chunk size
      int chunksPerTurn = (int) ceil(neededPerTurn / perChunk);
      neededPerTurn = chunksPerTurn * perChunk;
      if (0 < reserveLabour) {
	neededPerTurn -= reserveLabour;
	reserveLabour -= chunksPerTurn * perChunk;
      }
      if (1 == turns) bidlist.push_back(new MarketBid(TradeGood::Labor, neededPerTurn, this, 1));
      else bidlist.push_back(new MarketBid(TradeGood::Labor, neededPerTurn, this, turns-1));
    }

    double winterLabour = industry->getWinterLabour(prices, lastBlock, totalExpectedProduction, fullCycleLabour);
    if (reserveLabour > winterLabour) {
      reserveLabour -= winterLabour;
      winterLabour = 0;
    }
    else {
      winterLabour -= reserveLabour;
      reserveLabour = 0;
    }
    if (0 < winterLabour) bidlist.push_back(new MarketBid(TradeGood::Labor, winterLabour, this, 1));

    for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
      if (capital->getAmount(*tg) < 0.00001) continue;
      double laborSaving = totalLabourUsed * (1 - marginalCapFactor((*tg), getAmount(*tg)));
      // Assuming discount rate of 10%. Present value of amount x every period to infinity is (x/r) with r the interest rate.
      // TODO: Take decay into account. Variable discount rate?
      double npv = laborSaving * prices.getAmount(TradeGood::Labor) * 10;
      if (npv > prices.getAmount(*tg) * industry->getCapitalSize()) bidlist.push_back(new MarketBid((*tg), industry->getCapitalSize(), this, 1));
    }

    // Decide how much output to sell. On average, sell the inverse
    // of the length of the production cycle, thus keeping the output
    // constant. But sell more at higher prices. Expect that in the
    // long run, there will be a particular marginal ratio of labour
    // to food; when the current marginal ratio is higher (we are getting
    // more food per labour) then the price of food is low, so reduce
    // sales; and vice-versa.
    if (1 > getAmount(output)) return;
    marginalLabourRatio *= inverseExpectedRatio;

    // Should equal 1 when ratio is 1, approach 3 (make adjustable?) asymptotically upwards, and 0 downwards.
    double fractionToSell = 1 + (marginalLabourRatio >= 1 ? 2 : 1) * atan(marginalLabourRatio) * M_2_PI;
    fractionToSell *= inverseProductionTime;
    fractionToSell *= getAmount(output);
    fractionToSell -= soldThisTurn.getAmount(output);
    fractionToSell -= promisedToDeliver.getAmount(output);
    if (1 > fractionToSell) return;
    bidlist.push_back(new MarketBid(output, -1 * min(fractionToSell, getAmount(output)), this, 1));
  }
  
  double capitalFactor (const GoodsHolder& capitalToUse) const {
    double ret = 1;
    for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
      ret *= capFactor(capital->getAmount(*tg), capitalToUse.getAmount(*tg)/industry->getCapitalSize());
    }
    return ret;
  }
  
protected:
  double extraLabourFactor (double extraLabour, double regularLabour) const {return sqrt(1 + extraLabour / (0.000001 + regularLabour));}

  BlockInfo* const blockInfo;
  // Capital reduces the amount of labour required by factor (1 - x log (N+1)). This array stores x. 
  static GoodsHolder* capital;
  static TradeGood const* output;
  
private:
  T* industry;

  double capFactor (double reductionConstant, double goodAmount) const {
    return max(0.001, 1 - reductionConstant * log(1 + goodAmount));
  }

  // Return the reduction in required labor if we had one additional unit.
  double marginalCapFactor (TradeGood const* const tg, double currentAmount) const {
    double dilution = 1.0 / industry->getCapitalSize();
    return capFactor(capital->getAmount(tg), (industry->getCapitalSize()+currentAmount)*dilution) / capFactor(capital->getAmount(tg), currentAmount*dilution);
  }

  static double inverseProductionTime;
};

template<class T> GoodsHolder* Industry<T>::capital = 0;
template<class T> TradeGood const* Industry<T>::output = 0;
template<class T> double Industry<T>::inverseProductionTime = 1;

class Building : public BlockInfo {
  friend class StaticInitialiser; 
public: 
  Building (double mf = 1, int bs = 1, int wb = 1) : BlockInfo(mf, bs, wb), owner(0) {}
  ~Building () {}
  
  virtual void endOfTurn () = 0; 
  virtual void setOwner (Player* p); 
  Player* getOwner () {return owner;} 
  int getAssignedLand () const {return assignedLand;}
  void assignLand (int amount) {assignedLand += amount; if (0 > assignedLand) assignedLand = 0;}

  // For use in statistics.
  virtual double expectedProduction () const {return 0;}
  virtual double possibleProductionThisTurn () const {return 0;}

protected:
  
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
  virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist);
  Line* getLocation () const {return location;}
  MilUnit* getGarrison (unsigned int i) {if (i >= garrison.size()) return 0; return garrison[i];}
  Hex* getSupport () {return support;}
  int numGarrison () const {return garrison.size();}
  void recruit (Outcome out);  
  MilUnit* removeGarrison ();
  MilUnit* removeUnit (MilUnit* r);
  virtual void setOwner (Player* p);
  void setRecruitType (const MilUnitTemplate* m) {recruitType = m;}
  const MilUnitTemplate* getRecruitType () const {return recruitType;}
  virtual void setMirrorState ();
  
  static const int maxGarrison; 
  static double getSiegeMod () {return siegeModifier;}
  static void unitTests ();

protected:
  
private:
  Castle (Castle* other);   

  void deliverToUnit (MilUnit* unit, const GoodsHolder& goods);
  void distributeSupplies ();

  vector<MilUnit*> garrison;   // Units within the castle, drawing on its supplies.
  vector<MilUnit*> fieldForce; // Units outside, but still supported from here.
  map<MilUnit*, GoodsHolder> orders;
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
  friend class VillageGraphicsInfo;
public:
  Village ();
  ~Village ();

  double consumption () const;
  void demobMilitia ();
  virtual void endOfTurn ();
  virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist);
  double getFractionOfMaxPop () const {double ret = getTotalPopulation(); ret /= maxPopulation; return min(1.0, ret);}
  VillageGraphicsInfo* getGraphicsInfo () const {return graphicsInfo;}
  MilitiaTradition* getMilitia () {return milTrad;} 
  const MilUnitGraphicsInfo* getMilitiaGraphics () const; 
  int getTotalPopulation () const {return males.getTotalPopulation() + women.getTotalPopulation();}
  MilUnit* raiseMilitia ();
  virtual double produceForContract (TradeGood const* const tg, double amount);
  virtual double produceForTaxes (TradeGood const* const tg, double amount, ContractInfo::AmountType taxType);
  int produceRecruits (MilUnitTemplate const* const recruitType, MilUnit* target, Outcome dieroll);
  double production () const;
  virtual void setMirrorState ();  
  void increaseTradition (MilUnitTemplate const* target = 0) {milTrad->increaseTradition(target);} 
  string getBidStatus () const;
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
    void normalise ();
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
  VillageGraphicsInfo* graphicsInfo;
  static int maxPopulation; 
  static vector<double> baseMaleMortality;
  static vector<double> baseFemaleMortality;
  static vector<double> pairChance;
  static vector<double> fertility;
};

template <class W, class S, int N> class Collective {
  friend class StaticInitialiser;
public:
  Collective () {}
  ~Collective () {BOOST_FOREACH(W* worker, workers) worker->destroyIfReal();}

  typedef W WorkerType;
  typedef S StatusType;

  void doWork (bool tick = false) {BOOST_FOREACH(W* worker, workers) worker->extractResources(tick);}
  double getAmount (TradeGood const* const tg) {double ret = 0; BOOST_FOREACH(W* f, workers) ret += f->getAmount(tg); return ret;}
  GoodsHolder loot (double lootRatio) {GoodsHolder ret; BOOST_FOREACH(W* worker, workers) ret += worker->loot(lootRatio); return ret;}
  void setMarket (Market* market) {BOOST_FOREACH(W* worker, workers) market->registerParticipant(worker);}

  void setDefaultOwner (EconActor* o) {
    if (!o) return;
    BOOST_FOREACH(W* worker, workers) {  
      if (worker->getEconOwner()) continue;
      worker->setEconOwner(o);
    }
  }

protected:
  vector<W*> workers;
  static const int numOwners = N;

  template<class C> static void createWorkers (C* collective) {
    for (int i = 0; i < C::numOwners; ++i) {
      collective->workers.push_back(new typename C::WorkerType(collective));
    }
  }
};

class FieldStatus : public Enumerable<const FieldStatus> {
  friend class StaticInitialiser;
public:
  FieldStatus (string n, int sprl, int suml, int autl, int winl, int y);
  ~FieldStatus ();

  static Iter startPlow () {return plowing.begin();}
  static Iter finalPlow () {return plowing.end();}
  static Iter startWeed () {return weeding.begin();}
  static Iter finalWeed () {return weeding.end();}
  static Iter startReap () {return harvest.begin();}
  static Iter finalReap () {return harvest.end();}

  int springLabour;
  int summerLabour;
  int autumnLabour;
  int winterLabour;
  int yield;

private:
  static void clear();

  static vector<const FieldStatus*> plowing;
  static vector<const FieldStatus*> weeding;
  static vector<const FieldStatus*> harvest;
};

class Farmer : public Industry<Farmer>, public Mirrorable<Farmer> {
  friend class StaticInitialiser;
  friend class Mirrorable<Farmer>;
  friend class Farmland;
public:
  Farmer (Farmland* b);
  ~Farmer ();
  double outputOfBlock (int b) const;
  double getCapitalSize () const;
  void getLabourForBlock (int block, vector<jobInfo>& jobs, double& prodCycleLabour) const;
  double getWinterLabour (const GoodsHolder& prices, int lastBlock, double expectedProd, double expectedLabour) const;
  int numBlocks () const;
  virtual void setMirrorState ();
  void unitTests ();
  void extractResources (bool tick = false);
private:
  Farmer(Farmer* other);
  void fillBlock (int block, vector<int>& theBlock) const;
  string writeFieldStatus () const;
  vector<int> fields;
  double extraLabour;
  double totalWorked;

  static int _labourToClear;
};

class Farmland : public Building, public Mirrorable<Farmland>, public Collective<Farmer, FieldStatus, 10> {
  friend class Mirrorable<Farmland>;
  friend class StaticInitialiser;
  friend class FarmGraphicsInfo;
  friend class Farmer;
public:
  Farmland ();
  ~Farmland ();

  void devastate (int devastation);
  virtual void endOfTurn ();  
  virtual void setMirrorState ();
  int getFieldStatus (FieldStatus const* const fs) {return totalFields[*fs];}
  int getTotalFields () const;

  virtual double expectedProduction () const;
  virtual double possibleProductionThisTurn () const;

  static Farmland* getTestFarm (int numFields = 0);
  static void unitTests ();

private:
  Farmland (Farmland* other);
  void countTotals ();
  vector<int> totalFields; // Sum over farmers.
};

class ForestStatus : public Enumerable<const ForestStatus> {
  friend class StaticInitialiser;
public:
  ForestStatus (string n, int y, int tl, int hl);
  ~ForestStatus ();

  int yield;
  int labourToTend;
  int labourToHarvest;
};

class Forester : public Industry<Forester>, public Mirrorable<Forester> {
  friend class StaticInitialiser;
  friend class Mirrorable<Forester>;
  friend class Forest;
public:
  Forester (Forest* b);
  ~Forester ();
  double outputOfBlock (int b) const;
  double getCapitalSize () const;
  void getLabourForBlock (int block, vector<jobInfo>& jobs, double& prodCycleLabour) const;
  double getWinterLabour (const GoodsHolder& prices, int lastBlock, double expectedProd, double expectedLabour) const {return 0;}
  int numBlocks () const;
  virtual void setMirrorState ();
  void unitTests ();
  void extractResources (bool tick);

  vector<int> fields;
  vector<ForestStatus const*> myBlocks;
  int tendedGroves;
  int wildForest;
private:
  Forester(Forester* other);
  int getForestArea () const;
  int getTendedArea () const;
  void createBlockQueue ();
};

class Forest : public Building, public Mirrorable<Forest>, public Collective<Forester, ForestStatus, 10> {
  friend class StaticInitialiser;
  friend class Mirrorable<Forest>;
  friend class Forester;
public:
  Forest ();
  ~Forest ();

  virtual void endOfTurn ();
  virtual void setMirrorState ();

  static Forest* getTestForest () {return new Forest();}
  static void unitTests ();
  
private:
  Forest (Forest* other);
  int yearsSinceLastTick;

  static int _labourToClear;   // Go from wild to clear.
};

class MineStatus : public Enumerable<MineStatus> {
  friend class StaticInitialiser;
public:
  MineStatus (string n, int rl, int wl);
  ~MineStatus ();
  int requiredLabour;
  int winterLabour;
};

class Miner : public Industry<Miner>, public Mirrorable<Miner> {
  friend class Mirrorable<Miner>;
  friend class StaticInitialiser;
  friend class Mine;
public:
  Miner (Mine* m);
  ~Miner ();
  double outputOfBlock (int b) const;
  double getCapitalSize () const;
  void getLabourForBlock (int block, vector<jobInfo>& jobs, double& prodCycleLabour) const;
  double getWinterLabour (const GoodsHolder& prices, int lastBlock, double expectedProd, double expectedLabour) const;
  int numBlocks () const;
  virtual void setMirrorState ();
  void unitTests ();
  void extractResources (bool tick = false);
  
  vector<int> fields;
private:
  Miner(Miner* other);
};


class Mine : public Building, public Mirrorable<Mine>, public Collective<Miner, MineStatus, 10> {
  friend class StaticInitialiser;
  friend class Mirrorable<Mine>;
  friend class Miner;
public:
  Mine ();
  ~Mine ();

  virtual void endOfTurn ();
  virtual void setMirrorState ();

  static Mine* getTestMine () {return new Mine();}
  static void unitTests ();
private: 
  Mine (Mine* other);
  static int _amountOfIron;
};

#endif
