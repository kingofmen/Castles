#ifndef MIRRORABLE_HH
#define MIRRORABLE_HH

class Player; 
#include <map> 

struct AiValue {
  // POD type for holding AI values with meaningful names. 
  AiValue ()
    : influence(0)
    , strategic(0)
  {}
  double influence;
  double strategic;
  std::map<Player*, int> distanceMap; 
  std::map<Player*, double> influenceMap; 
  
  void clearFully () {
    strategic = 1;
    distanceMap.clear();
    influenceMap.clear(); 
  }
};

template <class T> class Mirrorable {
public:
  Mirrorable () : mirror(0) {} 
  virtual ~Mirrorable () {if (mirror) delete mirror;}
  virtual void setMirrorState () = 0; 
  T* getMirror () {return mirror;}
  void destroyIfReal () {if (mirror) delete this;}
  AiValue value; 
  
protected:
  T* mirror; 
private:
};

#endif
