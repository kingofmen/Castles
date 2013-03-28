#ifndef POPUNIT_HH
#define POPUNIT_HH

#include <vector> 
#include <list> 
#include <map> 
#include "Hex.hh" 
#include "Mirrorable.hh" 

class Player; 

class Unit {
public: 
  Unit ();
  ~Unit();

  void setOwner (Player* p) {player = p;}
  Player* getOwner () {return player;}
private:
  Player* player; 
};

class PopUnit : public Unit {
public: 
  PopUnit ();
  ~PopUnit (); 
  
private:

};

#endif 
