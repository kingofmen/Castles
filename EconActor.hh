#ifndef ECONACTOR_HH
#define ECONACTOR_HH

#include <vector>
#include <string>
#include <cmath>
using namespace std; 
class EconActor; 

struct ContractInfo {
  ContractInfo () : amount(0), delivery(Fixed) {}
  
  enum AmountType {Fixed, Percentage, SurplusPercentage};
  unsigned int good; 
  double amount;
  AmountType delivery;
  double delivered;
  EconActor* recipient; 
};

struct Bid {
  EconActor* actor; 
  unsigned int good;
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

  unsigned int good; 
  double amount; // Amount to count as "one unit" for purposes of taking the log. 
};

class Consumer {
public:  
  friend class StaticInitialiser;
  
protected:
  void setUtilities (vector<vector<Utility> >& needs, double* goods, double consumption); 
  
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

class EconActor {
  friend class StaticInitialiser; 
  friend class Market; 
  
public: 
  EconActor ();
  ~EconActor ();

  void         deliverGoods   (unsigned int good, double amount) {goods[good] += amount;}
  virtual void getBids        (const vector<double>& prices, vector<Bid>& wantToBuy, vector<Bid>& wantToSell);
  int          getId          () const {return id;}   
  virtual void produce        () {}
  
  typedef vector<EconActor*>::iterator Iter;
  static Iter start () {return allActors.begin();}
  static Iter final () {return allActors.end();} 

  static void executeContracts ();   
  static EconActor* getById (int id);
  static unsigned int getIndex (string name);
  static string getGoodName (unsigned int idx) {return goodNames[idx];}
  static unsigned int getNumGoods () {return numGoods;} 
  static void production (); 
  static void setAllUtils ();

  static const unsigned int Money;
  static const unsigned int Labor; 

protected:
  virtual void setUtilities (); 
  
  double* goods;
  vector<vector<Utility> > needs; 
  static unsigned int numGoods;
  static vector<string> goodNames;
  
private:
  int id;
  vector<ContractInfo*> obligations; 
  static vector<EconActor*> allActors; 
};

template<class T> class Industry {
  friend class StaticInitialiser;
  
public:
  virtual void marginalOutput (unsigned int good, int owner, double** output) = 0; // Returns additional expected output for one unit of given input. 
  
protected:
  double capitalFactor (double* goods, int dilution = 1) const {
    double ret = 1;
    for (unsigned int g = 0; g < EconActor::getNumGoods(); ++g) ret *= (1 - capital[g]*log(goods[g]/dilution+1)); 
    return ret;
  }
  
  // Capital reduces the amount of labour required by factor (1 - x log (N+1)). This array stores x. 
  static double* capital; 
}; 

template<class T> double* Industry<T>::capital; 

#endif 
