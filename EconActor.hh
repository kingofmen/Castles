#ifndef ECONACTOR_HH
#define ECONACTOR_HH

#include <vector>
#include <string> 
using namespace std; 

class EconActor {
  friend class StaticInitialiser; 
  
public: 
  EconActor ();
  ~EconActor ();

  int getId () const {return id;} 
  void deliverGoods (unsigned int good, double amount) {goods[good] += amount;} 
  
  //virtual void getBids (vector<pair<double, double> >& wantToBuy, ) {} 
  static EconActor* getById (int id);
  static unsigned int getIndex (string name); 
  static const unsigned int Money;
  static const unsigned int Labor; 
  
private:
  int id;
  double* goods;
  static int numGoods;
  static vector<string> goodNames; 
  static vector<EconActor*> allActors; 
};

#endif 
