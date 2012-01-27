#ifndef BUILDING_HH
#define BUILDING_HH

#include <vector> 
#include "Mirrorable.hh" 

class MilUnit; 
class Hex; 
class Player; 

class Building {
public: 


  virtual void setOwner (Player* p) {owner = p;}
  Player* getOwner () {return owner;} 

private:
  Player* owner; 
};

class Castle : public Building, public Mirrorable<Castle> {
  friend class Mirrorable<Castle>; 
public: 
  Castle (Hex* dat);
  ~Castle (); 

  int numGarrison () const {return garrison.size();}
  Hex* getSupport () {return support;}
  MilUnit* removeGarrison () {MilUnit* ret = garrison.back(); garrison.pop_back(); return ret;}
  MilUnit* removeUnit (MilUnit* r); 
  void addGarrison (MilUnit* p);
  virtual void setOwner (Player* p);
  MilUnit* recruit ();
  virtual void setMirrorState ();
  int getRecruitState () const {return recruited;} 
  
  static const int maxGarrison; 
  static const int maxRecruits; 
  
private:
  Castle (Castle* other);   
  
  std::vector<MilUnit*> garrison;
  Hex* support;
  int recruited;
}; 




#endif
