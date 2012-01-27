#include "RiderGame.hh"
#include <QtCore> 
#include <ctype.h>
#include <list> 
#include "PopUnit.hh"
#include "MilUnit.hh" 
#include "Hex.hh" 
#include "Player.hh"
#include <set>
#include "Logger.hh" 
#include "Building.hh" 
#include <cassert> 
#include <ctime>
#include "Parser.hh" 
#include <fstream> 

WarfareGame* WarfareGame::currGame = 0; 

WarfareGame::WarfareGame () {
  // Clear hexes, vertices, etc
}
WarfareGame::~WarfareGame () {
  Vertex::clear();
  Hex::clear();
  Line::clear();
  Player::clear(); 
  currGame = 0; 
}

Hex* WarfareGame::getHex (int x, int y) {
  for (std::vector<Hex*>::iterator i = hexes.begin(); i != hexes.end(); ++i) {
    if ((*i)->getPos().first != x) continue;
    if ((*i)->getPos().second != y) continue;
    return (*i); 
  }
  return 0; 
}

Hex* WarfareGame::findHex (Object* info) {
  int x = info->safeGetInt("x", -1);
  int y = info->safeGetInt("y", -1);
  assert(x >= 0);
  assert(y >= 0);
  assert(currGame); 
  Hex* hex = currGame->getHex(x, y);
  assert(hex);
  return hex; 
}

Line* WarfareGame::findLine (Object* info, Hex* hex) {
  std::string pos = info->safeGetString("pos", "nowhere");
  Hex::Direction dir = Hex::getDirection(pos);
  assert(Hex::None != dir);
  assert(hex->getLine(dir)); 
  return hex->getLine(dir); 
}

Vertex* WarfareGame::findVertex (Object* info, Hex* hex) {
  std::string pos = info->safeGetString("vtx", "nowhere");
  Hex::Vertices dir = Hex::getVertex(pos); 
  assert(Hex::NoVertex != dir);
  assert(hex->getVertex(dir)); 
  return hex->getVertex(dir); 
}

WarfareGame* WarfareGame::createGame (std::string filename, Player*& currplayer) {
  srand(time(NULL));
  if (currGame) delete currGame; 
  currGame = new WarfareGame();
  Object* game = processFile(filename); 
  assert(game); 
  Object* hexgrid = game->safeGetObject("hexgrid");
  assert(hexgrid);
  int xsize = hexgrid->safeGetInt("x", -1);
  int ysize = hexgrid->safeGetInt("y", -1);
  assert(xsize > 0);
  assert(ysize > 0); 
  for (int i = 0; i < xsize; ++i) {
    for (int j = 0; j < ysize; ++j) {
      currGame->hexes.push_back(new Hex(i, j, Hex::Plain)); 
    }
  }

  for (int i = 0; i < xsize; ++i) {
    for (int j = 0; j < ysize; ++j) {
      Hex* curr = currGame->getHex(i, j);
      for (int k = Hex::NorthWest; k < Hex::None; ++k) {
	std::pair<int, int> n = Hex::getNeighbourCoordinates(curr->getPos(), Hex::convertToDirection(k));
	curr->setNeighbour(Hex::convertToDirection(k), currGame->getHex(n.first, n.second));
      }
    }
  }
  
  for (std::vector<Hex*>::iterator i = currGame->hexes.begin(); i != currGame->hexes.end(); ++i) {
    (*i)->createVertices(); 
  }

  for (Vertex::Iterator vex = Vertex::begin(); vex != Vertex::end(); ++vex) {
    (*vex)->createLines(); 
  }

  objvec players = game->getValue("faction");
  Player* currp = 0; 
  for (objiter p = players.begin(); p != players.end(); ++p) {
    bool human = ((*p)->safeGetString("human", "no") == "yes");
    std::string display = (*p)->safeGetString("displayname"); 
    std::string name = (*p)->safeGetString("name");
    assert(display != "");
    assert(name != "");
    if (std::string::npos != display.find('"')) {
      display = display.substr(1+display.find('"'));
    }
    if (std::string::npos != display.rfind('"')) {
      display = display.substr(0, display.rfind('"'));
    }
    
    currp = new Player(human, display, name);
  }
  
  objvec hexinfos = game->getValue("hexinfo");
  for (objiter hinfo = hexinfos.begin(); hinfo != hexinfos.end(); ++hinfo) {
    Hex* hex = findHex(*hinfo);

    int dev = (*hinfo)->safeGetInt("devastation");
    for (int i = 0; i < dev; ++i) hex->raid();
    
    Object* cinfo = (*hinfo)->safeGetObject("castle");
    if (cinfo) {
      Castle* castle = new Castle(hex);
      std::string ownername = cinfo->safeGetString("player");
      Player* owner = Player::findByName(ownername);
      assert(owner);
      castle->setOwner(owner);
      hex->setOwner(owner);
      int numGarrison = cinfo->safeGetInt("garrison", 0);
      for (int i = 0; i < numGarrison; ++i) {
	MilUnit* m = new MilUnit();
	m->setOwner(owner);
	castle->addGarrison(m); 
      }
      int rec = cinfo->safeGetInt("recruit", 0);
      for (int i = 0; i < rec; ++i) castle->recruit(); 
      Line* lin = findLine(cinfo, hex);
      assert(0 == lin->getCastle()); 
      lin->addCastle(castle); 
    }
  }

  objvec units = game->getValue("unit");
  for (objiter unit = units.begin(); unit != units.end(); ++unit) {
    MilUnit* u = new MilUnit();
    Player* owner = Player::findByName((*unit)->safeGetString("player"));
    assert(owner);
    u->setOwner(owner);
    Hex* hex = findHex(*unit);
    Vertex* vtx = findVertex(*unit, hex);
    vtx->addUnit(u);
    if ((*unit)->safeGetString("weak", "no") == "yes") u->weaken(); 
  }

  currplayer = Player::findByName(game->safeGetString("currentplayer"));
  assert(currplayer); 
  
  return currGame; 
}

void WarfareGame::endOfTurn () {
  
}

void WarfareGame::saveGame (std::string fname, Player* currentplayer) {
  if (!currGame) return;

  Object* game = new Object("toplevel");
  Parser::topLevel = game;

  for (Player::Iterator p = Player::begin(); p != Player::end(); ++p) {
    Object* faction = new Object("faction");
    faction->setLeaf("name", (*p)->getName());
    faction->setLeaf("displayname", std::string("\"") + (*p)->getDisplayName() + "\"");
    faction->setLeaf("human", (*p)->isHuman() ? "yes" : "no");
    game->setValue(faction); 
  }

  game->setLeaf("currentplayer", currentplayer->getName()); 
  
  std::map<Hex*, Object*> hexmap; 
  int maxx = -1;
  int maxy = -1;
  for (Hex::Iterator hex = Hex::begin(); hex != Hex::end(); ++hex) {
    maxx = std::max((*hex)->getPos().first, maxx);
    maxy = std::max((*hex)->getPos().second, maxy);
    if (0 < (*hex)->getDevastation()) {
      Object* hinfo = new Object("hexinfo");
      hinfo->setLeaf("x", (*hex)->getPos().first);
      hinfo->setLeaf("y", (*hex)->getPos().second);
      hinfo->setLeaf("devastation", (*hex)->getDevastation());
      hexmap[*hex] = hinfo; 
    }
  }

  Object* hexgrid = new Object("hexgrid");
  hexgrid->setLeaf("x", maxx+1);
  hexgrid->setLeaf("y", maxy+1);
  game->setValue(hexgrid); 

  for (Line::Iterator lin = Line::begin(); lin != Line::end(); ++lin) {
    Castle* curr = (*lin)->getCastle();
    if (!curr) continue; 
    Hex* sup = curr->getSupport();
    Object* hinfo = hexmap[sup];
    if (!hinfo) {
      hinfo = new Object("hexinfo");
      hexmap[sup] = hinfo;
      hinfo->setLeaf("x", sup->getPos().first);
      hinfo->setLeaf("y", sup->getPos().second);
    }
    Object* castle = new Object("castle");
    hinfo->setValue(castle);
    castle->setLeaf("player", curr->getOwner()->getName());
    castle->setLeaf("pos", Hex::getDirectionName(sup->getDirection(*lin)));
    castle->setLeaf("garrison", curr->numGarrison());
    castle->setLeaf("recruit", curr->getRecruitState()); 
  }

  for (std::map<Hex*, Object*>::iterator hex = hexmap.begin(); hex != hexmap.end(); ++hex) {
    game->setValue((*hex).second); 
  }
  
  for (Vertex::Iterator vtx = Vertex::begin(); vtx != Vertex::end(); ++vtx) {
    if (0 == (*vtx)->numUnits()) continue;
    MilUnit* unit = (*vtx)->getUnit(0); 
    Object* uinfo = new Object("unit");
    int counter = 0;
    Hex* hex = (*vtx)->getHex(counter++);
    while (!hex) {
      hex = (*vtx)->getHex(counter++);
      assert(counter <= 6); 
    }
    uinfo->setLeaf("x", hex->getPos().first);
    uinfo->setLeaf("y", hex->getPos().second);
    uinfo->setLeaf("player", unit->getOwner()->getName());
    uinfo->setLeaf("vtx", Hex::getVertexName(hex->getDirection(*vtx)));
    if (unit->weakened()) uinfo->setLeaf("weak", "yes");
    game->setValue(uinfo); 
  }

  std::ofstream writer;
  writer.open(fname.c_str());
  writer << *game << "\n";
  writer.close(); 
  
}
