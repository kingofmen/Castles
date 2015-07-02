#ifndef ECONACTOR_HH
#define ECONACTOR_HH

#include <vector>
#include <string>
#include <cmath>
#include "UtilityFunctions.hh"
using namespace std; 
class EconActor;
class Market;
class MarketBid;
class MarketContract;

class TradeGood : public Enumerable<const TradeGood> {
  friend class StaticInitialiser; 
public:
  TradeGood (string n, bool lastOne=false);
  ~TradeGood ();
  
  static TradeGood const* Money;
  static TradeGood const* Labor;

  static Iter exMoneyStart() {Iter r = start();      return ++r;}
  static Iter exLaborStart() {Iter r = start(); ++r; return ++r;}

  double getStickiness () const {return stickiness;}
  double getConsumption () const {return consumption;}
  
private:
  static void initialise ();
  double stickiness;   // Ratio of maximum price drop to maximum price rise.
  double decay;        // Fractional storage loss per turn.
  double consumption;  // Amount lost when used for consumption.
  double capital;      // Amount lost when used for capital.
}; 

struct GoodsHolder {
public:
  GoodsHolder ();
  GoodsHolder (const GoodsHolder& other);
  double       getAmount    (unsigned int idx) const {return tradeGoods[idx];}
  double       getAmount    (TradeGood const* const tg) const {return tradeGoods[*tg];}
  void         deliverGoods (TradeGood const* const tg, double amount) {tradeGoods[*tg] += amount;}
  void         deliverGoods (const GoodsHolder& gh);
  string       display      (int indent = 0) const;
  GoodsHolder  loot         (double lootRatio);
  void         setAmount    (TradeGood const* const tg, double amount) {tradeGoods[*tg] = amount;}
  void         setAmounts   (GoodsHolder const* const gh);
  void         setAmounts   (const GoodsHolder& gh);
  void         zeroGoods    ();

  void operator-= (const GoodsHolder& other);
  void operator+= (const GoodsHolder& other);
  void operator*= (const double scale);
private:
  vector<double> tradeGoods;
};

GoodsHolder operator* (const GoodsHolder& gh, const double scale);
GoodsHolder operator* (const double scale, const GoodsHolder& gh);
double operator* (const GoodsHolder& gh1, const GoodsHolder& gh2);

struct ContractInfo : public Iterable<ContractInfo> {
  ContractInfo () : Iterable<ContractInfo>(this) {}
  void execute () const;
  enum AmountType {Fixed, Percentage};
  TradeGood const* tradeGood;
  double amount;
  AmountType delivery;
  double delivered;
  EconActor* recipient;
  EconActor* source;
};

struct Bid {
  EconActor* actor; 
  TradeGood const* tradeGood;
  double amount;
  double price; 
};

class EconActor : public Numbered<EconActor>, public GoodsHolder {
  friend class StaticInitialiser; 
  friend class Market; 

public: 
  EconActor ();
  ~EconActor ();

  void addObligation (ContractInfo* ci) {obligations.push_back(ci);}
  double availableCredit (EconActor* const applicant) const;
  void dunAndPay ();
  double extendCredit (EconActor* const applicant, double amountWanted);
  double getDiscountRate () const {return discountRate;}
  EconActor* getEconMirror () const {return econMirror;}
  void getPaid (EconActor* const payer, double amount);
  double getPromised (TradeGood const* const tg) const {return promisedToDeliver.getAmount(tg);}
  double getSold (TradeGood const* const tg) const {return soldThisTurn.getAmount(tg);}
  void leaveMarket ();
  virtual double produceForContract (TradeGood const* const tg, double amount);
  virtual double produceForTaxes (TradeGood const* const tg, double amount, ContractInfo::AmountType taxType);
  EconActor* getEconOwner () const {return owner;}
  bool isOwnedBy (EconActor const* const cand) const {return cand == owner;}
  void registerContract (MarketContract const* const contract);
  void setEconOwner (EconActor* ea) {owner = ea;}
  void setEconMirror (EconActor* ea) {econMirror = ea;}
  void unregisterContract (MarketContract const* const contract);
  
  virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {}
  static void clear () {Numbered<EconActor>::clear();}
  static void unitTests ();

protected:
  void clearRecord ();
  void registerSale (TradeGood const* const tg, double amount, double price);
  void setEconMirrorState (EconActor* ea);
  void produce (TradeGood const* const tg, double amount);
  void consume (TradeGood const* const tg, double amount);
  
  EconActor* owner;
  GoodsHolder soldThisTurn;
  GoodsHolder earnedThisTurn;
  GoodsHolder promisedToDeliver;
  Market* theMarket;
  EconActor* econMirror;
private:
  vector<ContractInfo*> obligations;
  map<EconActor*, double> borrowers;
  double discountRate;
};

#endif 
