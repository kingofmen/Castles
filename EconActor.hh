#ifndef ECONACTOR_HH
#define ECONACTOR_HH

#include <vector>
#include <string> 
using namespace std; 
class EconActor; 


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
  

  static EconActor* getById (int id);
  static unsigned int getIndex (string name);

  static const unsigned int Money;
  static const unsigned int Labor; 

protected:
  double* goods;
  vector<vector<Utility> > needs; 
  static unsigned int numGoods;
  static vector<string> goodNames;
  
private:
  int id;
  static vector<EconActor*> allActors; 
};

#endif 
