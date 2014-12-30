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

  double extendCredit (EconActor const* const applicant);
  void getPaid (EconActor* const payer, double amount);
  virtual double produceForContract (TradeGood const* const tg, double amount) {amount = min(amount, getAmount(tg)); deliverGoods(tg, -amount); return amount;}

  virtual void getBids      (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {}
  static void clear () {Numbered<EconActor>::clear();}
  static void unitTests ();
  
protected:

private:
  vector<ContractInfo*> obligations;
  map<EconActor const* const, double> borrowers;
};

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

#endif 
