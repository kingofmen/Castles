#ifndef ECONACTOR_HH
#define ECONACTOR_HH

#include <vector>
#include <string>
#include <cmath>
#include "UtilityFunctions.hh"
using namespace std; 
class EconActor; 
class MarketBid;

class TradeGood : public Enumerable<const TradeGood> {
  friend class StaticInitialiser; 
public:
  TradeGood (string n, bool lastOne=false);
  ~TradeGood ();
  
  static TradeGood const* Money;
  static TradeGood const* Labor;

  static Iter exMoneyStart() {Iter r = start();      return ++r;}
  static Iter exLaborStart() {Iter r = start(); ++r; return ++r;}
  
private:
  static void initialise ();
}; 

struct GoodsHolder {
public:
  GoodsHolder ();
  GoodsHolder (const GoodsHolder& other);
  void         clear        ();
  double       getAmount    (unsigned int idx) const {return tradeGoods[idx];}
  double       getAmount    (TradeGood const* const tg) const {return tradeGoods[*tg];}
  void         deliverGoods (TradeGood const* const tg, double amount) {tradeGoods[*tg] += amount;}
  void         setAmount    (TradeGood const* const tg, double amount) {tradeGoods[*tg] = amount;}
  void         setAmounts   (GoodsHolder const* const gh);
  void         setAmounts   (const GoodsHolder& gh);
private:
  vector<double> tradeGoods;
};

struct ContractInfo : public Iterable<ContractInfo> {
  ContractInfo () : Iterable<ContractInfo>(this) {}
  void execute () const;
  enum AmountType {Fixed, Percentage, SurplusPercentage};
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

  double availableCredit (EconActor* const applicant) const;
  void dunAndPay ();
  double extendCredit (EconActor* const applicant, double amountWanted);
  void getPaid (EconActor* const payer, double amount);
  virtual double produceForContract (TradeGood const* const tg, double amount) {amount = min(amount, getAmount(tg)); deliverGoods(tg, -amount); return amount;}
  EconActor* getEconOwner () const {return owner;}
  bool isOwnedBy (EconActor const* const cand) const {return cand == owner;}
  void setEconOwner (EconActor* ea) {owner = ea;}
  
  virtual void getBids      (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {}
  static void clear () {Numbered<EconActor>::clear();}
  static void unitTests ();
  
protected:
  EconActor* owner;

private:
  vector<ContractInfo*> obligations;
  map<EconActor*, double> borrowers;
};

#endif 
