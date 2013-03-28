#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector> 
class WarfareGame;
class Action; 
class MilUnit; 
class PlayerGraphicsInfo; 

class Player {
  friend class StaticInitialiser; 
public:
  Player (bool h, std::string d, std::string n);

  bool isHuman () const {return human;}
  void getAction (); 
  bool turnEnded () const {return doneWithTurn;}
  void finished () {doneWithTurn = true;} 
  void newTurn () {doneWithTurn = false;} 
  std::string getName () const {return name;}
  std::string getDisplayName () const {return displayName;} 
  PlayerGraphicsInfo const* getGraphicsInfo () const {return graphicsInfo;} 
  
  static Player* findByName (std::string n);   
  static Player* nextPlayer (Player* curr); 
  typedef std::vector<Player*>::iterator Iterator; 
  static Iterator begin () {return allPlayers.begin();}
  static Iterator end () {return allPlayers.end();} 
  static void clear (); 
  
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
  static std::vector<Player*> allPlayers;

  static double influenceDecay;
  static double castleWeight;
  static double casualtyValue; 
  static double distanceModifier;
  static double distancePower;
  static double supplyWeight;
  static double siegeInfluenceValue; 
}; 


#endif
