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

class MilUnit : public Unit, public Mirrorable<MilUnit> {
public:
  MilUnit ();
  ~MilUnit (); 

  void weaken ();
  void reinforce (); 
  bool weakened () const {return weak;} 
  virtual void setMirrorState ();
  void setRear (Hex::Vertices r) {rear = r;}
  Hex::Vertices getRear () const {return rear;} 
  
private:
  bool weak;
  Hex::Vertices rear; 
}; 

#endif 
