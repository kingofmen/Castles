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

  double production () const;
  double recruitsAvailable () const;
  bool recruit (double number); 
  double growth (Hex::TerrainType t);
  
private:
  double recruited; 
};

#endif 
