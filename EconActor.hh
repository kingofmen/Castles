#ifndef ECONACTOR_HH
#define ECONACTOR_HH

#include <vector>
#include <string> 
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

#endif 
