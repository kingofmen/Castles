#ifndef PLAYER_HH
#define PLAYER_HH

#include <string>
#include <vector> 
class WarfareGame;
class Action; 

class Player {
public:
  Player (bool h, std::string d, std::string n);

  bool isHuman () const {return human;}
  void getAction (WarfareGame* dat); 
  bool turnEnded () const {return doneWithTurn;}
  void finished () {doneWithTurn = true;} 
  void newTurn () {doneWithTurn = false;} 
  std::string getName () const {return name;}
  std::string getDisplayName () const {return displayName;} 
  
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
  double evaluate (Action act, WarfareGame* dat);
  double evaluateGlobalStrength (WarfareGame* dat);
  double evaluateAttackStrength (WarfareGame* dat, Player* att, Player* def);
  double calculateInfluence (); 
  static std::vector<Player*> allPlayers;

}; 


#endif
