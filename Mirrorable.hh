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
  Mirrorable (T* r = 0) : mirror(0), real(r) {
    if (!real) {
      real = static_cast<T*>(this);
      mirror = new T(real); 
    }
    else mirror = static_cast<T*>(this);
  } 
  virtual ~Mirrorable () {if (real == this) delete mirror;}
  virtual void setMirrorState () = 0; 
  T* getMirror () {return mirror;}
  T* getReal () {return real;} 
  void destroyIfReal () {if (isReal()) delete this;}
  AiValue value; 
  bool isMirror () const {return mirror == this;}
  bool isReal () const {return real == this;}   
  
protected:
  T* mirror;
  T* real; 
private:
};

template <class T, class K> void addContent (T* dis,
					     K* dat,
					     void (T::*fcn) (K*)) {
  if (dis->isReal()) (dis->*fcn)(dat->getReal());
  (dis->getMirror()->*fcn)(dat->getMirror());
}

#endif
