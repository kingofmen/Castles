#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector>
#include "EconActor.hh"
#include "UtilityFunctions.hh"
class WarfareGame;
class Action; 
class MilUnit; 
class PlayerGraphicsInfo; 

class Player : public Iterable<Player>, public Named<Player>, public EconActor {
  friend class StaticInitialiser; 
public:
  Player (bool h, std::string d, std::string n);
  ~Player ();

  bool isEnemy (Player const* const other) {return this != other;}
  bool isFriendly (Player const* const other) {return this == other;}
  bool isHuman () const {return human;}
  void getAction (); 
  bool turnEnded () const {return doneWithTurn;}
  void finished () {doneWithTurn = true;} 
  void newTurn () {doneWithTurn = false;} 
  std::string getDisplayName () const {return displayName;} 
  PlayerGraphicsInfo const* getGraphicsInfo () const {return graphicsInfo;} 

  static void clear (); 
  static void setCurrentPlayerByName (std::string name) {currentPlayer = findByName(name);}
  static void advancePlayer () {currentPlayer = nextPlayer();}
  static Player* getCurrentPlayer () {return currentPlayer;}
  static Player* nextPlayer ();
  static Player* getTestPlayer ();

  private: 
  bool human;
  bool doneWithTurn;
  std::string name;
  std::string displayName;
  PlayerGraphicsInfo* graphicsInfo; 
  
  double evaluate (Action act); 
  double evaluateGlobalStrength (); 
  double evaluateAttackStrength (Player* att, Player* def);
  double calculateInfluence ();
  double calculateUnitStrength (MilUnit* dat, double modifiers); 

  static Player* currentPlayer;
  static double influenceDecay;
  static double castleWeight;
  static double casualtyValue;
  static double distanceModifier;
  static double distancePower;
  static double supplyWeight;
  static double siegeInfluenceValue;
}; 

#endif
