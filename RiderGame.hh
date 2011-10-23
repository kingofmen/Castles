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

class WarfareGame {
public:
  ~WarfareGame ();

  Hex* getHex (int x, int y); 
  //static WarfareGame* createGame (QTextStream& input);
  static WarfareGame* createGame (std::string fname, Player*& currentplayer);
  static void saveGame (std::string fname, Player* currentplayer);
  
  typedef std::vector<Hex*>::iterator HexIterator;
  HexIterator begin() {return hexes.begin();}
  HexIterator end() {return hexes.end();} 
  void endOfTurn (); 
  
private:
  WarfareGame ();
  std::vector<Hex*> hexes;
  static WarfareGame* currGame;

  static Hex* findHex (Object* info);
  static Line* findLine (Object* info, Hex* hex);
  static Vertex* findVertex (Object* info, Hex* hex); 
}; 



#endif
