#ifndef WARFAREGAME_HH
#define WARFAREGAME_HH

#include <vector>
#include <list> 
#include <map>
#include <QtCore>
#include <string> 
#include "Hex.hh" 

class Hex;
class Vertex;
class Line; 
class Object; 
using namespace std; 

class WarfareGame {
public:
  ~WarfareGame ();

  static WarfareGame* createGame (std::string fname);
  
  void endOfTurn ();
  static void unitComparison (string fname);
  static void unitTests (string fname); 
  static void updateGreatestMilStrength ();    
  
private:
  WarfareGame ();
  static WarfareGame* currGame;

  void findCastles (vector<Castle*>& ret, Player* p);
  void findUnits (vector<MilUnit*>& ret, Player* p);
  
  static Hex* findHex (Object* info);
  static Line* findLine (Object* info, Hex* hex);
  static Vertex* findVertex (Object* info, Hex* hex); 
}; 



#endif
