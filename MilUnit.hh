#ifndef MILUNIT_HH
#define MILUNIT_HH

#include "PopUnit.hh"
#include <vector> 

struct MilUnitElement {
  std::string name;
  int shock;
  int fire;
  int defense;
  int tacmob;
  int strength; 
};

class MilUnit : public Unit, public Mirrorable<MilUnit> {
  friend class Mirrorable<MilUnit>; 
public:
  MilUnit ();
  ~MilUnit (); 

  void weaken ();
  void reinforce (); 
  bool weakened () const {return weak;} 
  virtual void setMirrorState ();
  void setRear (Hex::Vertices r) {rear = r;}
  Hex::Vertices getRear () const {return rear;} 

  int getScoutingModifier (MilUnit* const adversary);
  int getSkirmishModifier (MilUnit* const adversary);
  int getFightingModifier (MilUnit* const adversary);

  void battleCasualties (MilUnit* const adversary);
  void routCasualties (MilUnit* const adversary); 
  
private:
  MilUnit (MilUnit* other); 
  
  double calcStrength (double fraction, int MilUnitElement::*field);
  void takeCasualties (double rate);
  
  bool weak;
  Hex::Vertices rear;
  std::vector<MilUnitElement*> strength;
}; 




#endif
