#ifndef ECONACTOR_HH
#define ECONACTOR_HH

#include <vector>
#include <string> 
using namespace std; 

struct Bid {
  unsigned int good;
  double amount;
  double price; 
};

class Market {
  friend class StaticInitialiser; 
public:
  Market ();
  ~Market ();

  void findPrices (vector<Bid>& wantToBuy, vector<Bid>& wantToSell); 
  
protected:
  vector<double> prices; 
};

class EconActor {
  friend class StaticInitialiser; 
  friend class Market; 
  
public: 
  EconActor ();
  ~EconActor ();

  int getId () const {return id;} 
  void deliverGoods (unsigned int good, double amount) {goods[good] += amount;} 
  
  virtual void getBids (const vector<double>& prices, vector<Bid>& wantToBuy, vector<Bid>& wantToSell); 
  static EconActor* getById (int id);
  static unsigned int getIndex (string name); 
  static const unsigned int Money;
  static const unsigned int Labor; 

protected:
  double* goods;
  double* needs;
  static unsigned int numGoods;
  static vector<string> goodNames;
  
private:
  int id;
  static vector<EconActor*> allActors; 
};

#endif 
