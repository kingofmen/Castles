#ifndef ECONACTOR_HH
#define ECONACTOR_HH

#include <vector>
#include <string>
#include <cmath>
#include "UtilityFunctions.hh"
using namespace std; 
class EconActor; 
class TradeGood;

struct ContractInfo {
  enum AmountType {Fixed, Percentage, SurplusPercentage};
  TradeGood const* tradeGood;
  double amount;
  AmountType delivery;
  double delivered;
  EconActor* recipient; 
};

struct Bid {
  EconActor* actor; 
  TradeGood const* tradeGood;
  double amount;
  double price; 
};

struct Utility {
  Utility (double u, double m) : utility(u), margin(m) {} 
  double utility; // Per unit of goods
  double margin;  // Max amount at which we get this utility
};

struct MaslowNeed {
  // For storing the full hierarchy of utilities,
  // ie food then furniture then leisure. The Utility
  // class stores immediate marginal utilities, the things
  // to bid on this turn.

  TradeGood const* tradeGood;
  double amount; // Amount to count as "one unit" for purposes of taking the log. 
};

class Consumer {
public:  
  friend class StaticInitialiser;
  
protected:
  void setUtilities (vector<vector<Utility> >& needs, const vector<double>& goods, double consumption); 
  
private:
  static vector<vector<MaslowNeed> > hierarchy;
  static vector<double> levelAmounts; // Utility that counts as 100 percent for each level.   
};

class Market {
  friend class StaticInitialiser; 
public:
  Market ();
  ~Market ();

  void findPrices (vector<Bid>& wantToBuy, vector<Bid>& wantToSell); 
  void trade      (const vector<Bid>& wantToBuy, const vector<Bid>& wantToSell);
  
protected:
  vector<double> prices; 
};

class TradeGood : public Enumerable<const TradeGood> {
  friend class StaticInitialiser; 
public:
  TradeGood (string n, bool lastOne=false);
  ~TradeGood ();
  
  static TradeGood const* Money;
  static TradeGood const* Labor;

  static Iter exMoneyStart() {Iter r = start(); ++r; return r;}
  
private:
  static void initialise ();
}; 

class EconActor : public Iterable<EconActor>, public Numbered<EconActor> {
  friend class StaticInitialiser; 
  friend class Market; 
  
public: 
  EconActor ();
  ~EconActor ();

  double       getAmount      (unsigned int idx) const {return tradeGoods[idx];}
  double       getAmount      (TradeGood const* const tg) const {return tradeGoods[*tg];}
  void         deliverGoods   (TradeGood const* const tg, double amount) {tradeGoods[*tg] += amount;}
  virtual void getBids        (const vector<double>& prices, vector<Bid>& wantToBuy, vector<Bid>& wantToSell);
  virtual void produce        () {}

  static void clear ();  
  static void executeContracts ();
  static void production ();
  static void setAllUtils ();

  int creationOrder;
protected:
  virtual void setUtilities ();

  vector<double> tradeGoods;
  vector<vector<Utility> > needs;

private:
  vector<ContractInfo*> obligations;
};

template<class T> class Industry {
  friend class StaticInitialiser;

public:
  virtual void marginalOutput (unsigned int good, int owner, double** output) = 0; // Returns additional expected output for one unit of given input.

protected:
  double capitalFactor (double* goods, int dilution = 1) const {
    double ret = 1;
    for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
      ret *= (1 - capital[**tg]*log(goods[**tg]/dilution+1));
    }
    return ret;
  }
  
  // Capital reduces the amount of labour required by factor (1 - x log (N+1)). This array stores x. 
  static double* capital; 
}; 

template<class T> double* Industry<T>::capital; 

#endif 
