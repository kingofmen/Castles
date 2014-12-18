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
#include "StructUtils.hh" 
#include "StaticInitialiser.hh" 
#include "Calendar.hh" 
#include "Directions.hh" 

WarfareGame* WarfareGame::currGame = 0; 
bool testingBool = true;

WarfareGame::WarfareGame () {
  // Clear hexes, vertices, etc
}

WarfareGame::~WarfareGame () {
  Vertex::clear();
  Hex::clear();
  Line::clear();
  Player::clear();
  EconActor::clear();
  currGame = 0;
}

WarfareGame* WarfareGame::createGame (string filename) {
  Logger::logStream(DebugStartup) << "Entering createGame " << currGame << "\n";
  //srand(time(NULL));
  srand(42);
  if (currGame) delete currGame;
  Logger::logStream(DebugStartup) << "Creating new game\n";
  currGame = new WarfareGame();
  Logger::logStream(DebugStartup) << "Processing savegame\n";
  //assert(testingBool);   
  Object* game = processFile(filename); 
  assert(game);
  Logger::logStream(DebugStartup) << "Loading pop info\n";
  Object* popInfo = processFile("./common/popInfo.txt");
  assert(popInfo);
  Logger::logStream(DebugStartup) << "Loading goods\n";
  Object* goods = popInfo->safeGetObject("goods"); // Must come before any EconActors are created.
  Logger::logStream(DebugStartup) << "Initialising goods\n";
  StaticInitialiser::initialiseGoods(goods);
  Object* hexgrid = game->safeGetObject("hexgrid");
  assert(hexgrid);
  int xsize = hexgrid->safeGetInt("x", -1);
  int ysize = hexgrid->safeGetInt("y", -1);
  assert(xsize > 0);
  assert(ysize > 0);
  Logger::logStream(DebugStartup) << "Clearing geography\n";
  Hex::clear();
  Vertex::clear();
  Line::clear(); 

  Logger::logStream(DebugStartup) << "Creating geography\n";
  for (int i = 0; i < xsize; ++i) {
    for (int j = 0; j < ysize; ++j) {
      Hex::createHex(i, j, Plain); 
    }
  }

  Logger::logStream(DebugStartup) << "Creating actions\n";
  Object* actionInfo = processFile("./common/actions.txt");
  assert(actionInfo); 
  StaticInitialiser::createActionProbabilities(actionInfo);

  Logger::logStream(DebugStartup) << "Creating unit templates\n";
  Object* unitInfo = processFile("./common/units.txt");
  assert(unitInfo); 
  StaticInitialiser::buildMilUnitTemplates(unitInfo);
  Logger::logStream(DebugStartup) << "Initialising buildings\n";
  StaticInitialiser::initialiseCivilBuildings(popInfo);

  Logger::logStream(DebugStartup) << "Creating neighbours\n";
  for (int i = 0; i < xsize; ++i) {
    for (int j = 0; j < ysize; ++j) {
      Hex* curr = Hex::getHex(i, j);
      for (int k = NorthWest; k < NoDirection; ++k) {
	pair<int, int> n = Hex::getNeighbourCoordinates(curr->getPos(), convertToDirection(k));
	curr->setNeighbour(convertToDirection(k), Hex::getHex(n.first, n.second));
      }
    }
  }

  for (Hex::Iterator i = Hex::start(); i != Hex::final(); ++i) { 
    (*i)->createVertices(); 
  }

  for (Vertex::Iterator vex = Vertex::start(); vex != Vertex::final(); ++vex) {
    (*vex)->createLines(); 
  }

  HexGraphicsInfo::getHeights(); // Must come after Vertex and Line creation to get right zone width and height. 
  ZoneGraphicsInfo::calcGrid(); 
  StaticInitialiser::loadTextures(); 
  
  objvec players = game->getValue("faction");
  for (objiter p = players.begin(); p != players.end(); ++p) {
    StaticInitialiser::createPlayer(*p);
  }

  Object* aiInfo = processFile("./common/ai.txt"); 
  StaticInitialiser::loadAiConstants(aiInfo);
  StaticInitialiser::overallInitialisation(game); 

  objvec hexinfos = game->getValue("hexinfo");
  for (objiter hinfo = hexinfos.begin(); hinfo != hexinfos.end(); ++hinfo) {
    StaticInitialiser::buildHex(*hinfo); 
  }

  objvec units = game->getValue("unit");
  for (objiter unit = units.begin(); unit != units.end(); ++unit) {
    StaticInitialiser::buildMilUnit(*unit);
  }

  Player::setCurrentPlayerByName(game->safeGetString("currentplayer"));  
  assert(Player::getCurrentPlayer());
  updateGreatestMilStrength();
  return currGame; 
}

void WarfareGame::findUnits (vector<MilUnit*>& ret, Player* p) {
  for (MilUnit::Iterator m = MilUnit::begin(); m != MilUnit::end(); ++m) if ((*m)->getOwner() == p) ret.push_back(*m);
}

void WarfareGame::findCastles (vector<Castle*>& ret, Player* p) {
  for (Line::Iterator lin = Line::start(); lin != Line::final(); ++lin) {
    Castle* curr = (*lin)->getCastle();
    if (!curr) continue;
    if (curr->getOwner() != p) continue;
    ret.push_back(curr);
  }
}

struct Route {
  vector<Geography*> route;
  double distance;
  Castle* source;
  Vertex* target; 
};

map<Player*, map<Geography*, map<Geography*, Route*> > > routeMap;

void findRoute (Geography* source, Geography* destination, Player* side, double maxRisk) {
  set<Geography*> open;
  for (Vertex::Iterator v = Vertex::start(); v != Vertex::final(); ++v) (*v)->clearGeography();
  for (Line::Iterator l = Line::start(); l != Line::final(); ++l) (*l)->clearGeography();
  
  open.insert(source);
  source->distFromStart = 0;
  source->previous = 0;
  double inverseRisk = 1.0 / (1 - maxRisk); 
  while (0 < open.size()) {
    // Find lowest-cost node in open list
    Geography* bestOpen = (*(open.begin())); 
    double lowestEstimate = bestOpen->distFromStart + bestOpen->heuristicDistance(destination); 
    for (set<Geography*>::iterator g = open.begin(); g != open.end(); ++g) {
      // Pretend that all estimated traversals are maximally risky. This overestimates the cost
      // but that's fine for an A* heuristic. 
      double curr = (*g)->distFromStart + (*g)->heuristicDistance(destination) * inverseRisk; 
      if (lowestEstimate < curr) continue;
      bestOpen = (*g);
      lowestEstimate = curr; 
    }

    bestOpen->closed = true;
    open.erase(bestOpen); 
    vector<Geography*> neibs;
    bestOpen->getNeighbours(neibs);
    for (vector<Geography*>::iterator n = neibs.begin(); n != neibs.end(); ++n) {
      double currRisk = (*n)->traversalRisk(side);
      if (currRisk > maxRisk) {
	(*n)->closed = true;
	continue; 
      }
      double currCost = bestOpen->distFromStart + (*n)->traversalCost(side) / (1 - (*n)->traversalRisk(side));  
      if ((*n) == destination) {
	// Construct the Route and add it to routeMap;
	Route* ret = new Route();
	ret->distance = currCost; 
	routeMap[side][source][destination] = ret;
	ret->route.push_back(*n); 
	Geography* curr = bestOpen; 
	while (curr) {
	  ret->route.push_back(curr); 
	  ret->distance += curr->distFromStart;
	  curr = curr->previous;
	}
	return; 
      }
      if (currCost > (*n)->distFromStart) continue;

      (*n)->previous = bestOpen;
      (*n)->distFromStart = currCost;
      open.insert(*n); 
    }
  }
}

void WarfareGame::endOfTurn () {
  updateGreatestMilStrength();
  Hex::production();
  for (ContractInfo::Iter c = ContractInfo::start(); c != ContractInfo::final(); ++c) (*c)->execute();
  EconActor::utilityCallbacks.call();
  LineGraphicsInfo::endTurn(); 

  for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) (*hex)->endOfTurn();
  for (Line::Iterator lin = Line::start(); lin != Line::final(); ++lin) (*lin)->endOfTurn();
  for (Vertex::Iterator vex = Vertex::start(); vex != Vertex::final(); ++vex) (*vex)->endOfTurn();

  // TODO: Clear or refresh the cache (routeMaps) every so often. 
  
  // Trade
  for (Player::Iter p = Player::start(); p != Player::final(); ++p) {
    vector<Route*> routes;
    vector<Castle*> sources;
    vector<MilUnit*> sinks;
    findCastles(sources, (*p));
    findUnits(sinks, (*p));

    for (vector<Castle*>::iterator c = sources.begin(); c != sources.end(); ++c) {
      for (vector<MilUnit*>::iterator m = sinks.begin(); m != sinks.end(); ++m) {
	if (!(*m)->getLocation()) continue; // Indicates garrison unit 
	if (!routeMap[*p][(*c)->getLocation()][(*m)->getLocation()]) findRoute((*c)->getLocation(), (*m)->getLocation(), (*p), 0.8);
	routes.push_back(routeMap[*p][(*c)->getLocation()][(*m)->getLocation()]);
	routes.back()->source = (*c);
	routes.back()->target = (*m)->getLocation(); 
      }
    }

    for (vector<Castle*>::iterator c = sources.begin(); c != sources.end(); ++c) {
      (*c)->supplyGarrison(); 
    }

    sort(routes.begin(), routes.end(), deref<Route>(member_lt(&Route::distance)));
    for (vector<Route*>::iterator r = routes.begin(); r != routes.end(); ++r) {
      if (!(*r)) continue;
      double amountWanted = 0;
      for (Vertex::UnitIterator m = (*r)->target->beginUnits(); m != (*r)->target->endUnits(); ++m) {
	amountWanted += (*m)->getPrioritisedSuppliesNeeded(); 
      }
      if (amountWanted < 1) continue; 
      double amountSent = (*r)->source->removeSupplies(amountWanted); 

      Geography* previous = 0; 
      for (vector<Geography*>::reverse_iterator g = (*r)->route.rbegin(); g != (*r)->route.rend(); ++g) {
	(*g)->traverseSupplies(amountSent, *p, previous);
	previous = (*g); 
	//amountSent -= (*g)->traversalCost(*p);
	//Logger::logStream(DebugSupply) << "Traversed " << (int) (*g) << " at cost " << (*g)->traversalCost(*p) << "\n"; 
	//double lossChance = (*g)->traversalRisk(*p);
	//double roll = rand();
	//roll /= RAND_MAX;
	//if (roll < lossChance) amountSent *= 0.9; 
	if (amountSent < 0.05 * amountWanted) break;
      }
      if (amountSent < 0.05 * amountWanted) continue;

      for (Vertex::UnitIterator m = (*r)->target->beginUnits(); m != (*r)->target->endUnits(); ++m) {
	double fraction = (*m)->getPrioritisedSuppliesNeeded();
	fraction /= amountWanted;
	/*Logger::logStream(DebugSupply) << "Delivering to unit at "
				       << (*r)->target->getName() << " : "
				       << amountSent * fraction
				       << " from " << (*r)->source->getSupport()->getName()
				       << "\n"; */
	(*m)->addSupplies(amountSent * fraction); 
      }
    }
    
  }
  

  // Supply consumption, strength calculation
  for (MilUnit::Iterator mil = MilUnit::begin(); mil != MilUnit::end(); ++mil) (*mil)->endOfTurn(); 

  Calendar::newWeekBegins();
  Logger::logStream(Logger::Game) << Calendar::toString() << "\n";
  
  if (Calendar::Winter == Calendar::getCurrentSeason()) {
    // Hex buildings do special things in winter. 
    for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) (*hex)->endOfTurn();

    // So do MilUnits.
    for (MilUnit::Iterator mil = MilUnit::begin(); mil != MilUnit::end(); ++mil) (*mil)->endOfTurn();
    
    Calendar::newYearBegins(); 
  }
  FarmGraphicsInfo::updateFieldStatus();
  VillageGraphicsInfo::updateVillageStatus();
}

void WarfareGame::unitComparison (string fname) {
  Object* game = processFile(fname);
  StaticInitialiser::buildMilUnitTemplates(game->safeGetObject("unittypes"));

  vector<MilUnit*> units1;
  vector<MilUnit*> units2;
  AgeTracker recruits;
  for (MilUnitTemplate::Iterator t = MilUnitTemplate::begin(); t != MilUnitTemplate::end(); ++t) {
    MilUnit* u = new MilUnit();
    recruits.clear();
    recruits.addPop((*t)->recruit_speed, 18);
    u->addElement((*t), recruits); 
    units1.push_back(u);
    u->setName((*t)->name);

    u = new MilUnit();
    u->addElement((*t), recruits);     
    u->setName((*t)->name);    
    units2.push_back(u);    
  }

  for (vector<MilUnit*>::iterator unit1 = units1.begin(); unit1 != units1.end(); ++unit1) {
    for (vector<MilUnit*>::iterator unit2 = units2.begin(); unit2 != units2.end(); ++unit2) {

      (*unit1)->setAggression(0.99);
      (*unit2)->setAggression(0.25); 
      
      Logger::logStream(DebugStartup) << (*unit1)->getName() << " vs "
				      << (*unit2)->getName() << " : ("
				      << (*unit1)->effectiveMobility(*unit2) << " "
				      << (*unit1)->calcStrength((*unit1)->getDecayConstant(), &MilUnitElement::shock) << " "
				      << (*unit1)->calcStrength((*unit1)->getDecayConstant(), &MilUnitElement::range) << " "	
				      << (*unit1)->calcBattleCasualties(*unit2) << ") ("
				      << (*unit2)->effectiveMobility(*unit1) << " "
				      << (*unit2)->calcStrength((*unit2)->getDecayConstant(), &MilUnitElement::shock) << " "
				      << (*unit2)->calcStrength((*unit2)->getDecayConstant(), &MilUnitElement::range) << " "		
				      << (*unit2)->calcBattleCasualties(*unit1) << ")\n"; 
    }
  }
  
}

void WarfareGame::unitTests (string fname) {
  ofstream writer;
  writer.open("parseroutput.txt");
  setOutputStream(&writer);   
  Logger::logStream(DebugStartup) << "Test: Creating game from file " << fname << "... "; 
  WarfareGame* testGame = createGame(fname);
  Logger::logStream(DebugStartup) << "Passed.\n";
  Logger::logStream(DebugStartup) << "Test: EconActor contracts... ";
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    assert((*tg) != TradeGood::Money); 
  }
  Logger::logStream(DebugStartup) << "Passed.\n";
  Logger::logStream(DebugStartup) << "Test: Running a turn... ";
  testGame->endOfTurn();
  Logger::logStream(DebugStartup) << "Passed\n";  
  Logger::logStream(DebugStartup) << "Test: Writing game to file... ";
  StaticInitialiser::writeGameToFile(".\\savegames\\testsave.txt");
  Logger::logStream(DebugStartup) << "Passed\n";
  Logger::logStream(DebugStartup) << "Test: Loading from savegame again... ";
  testingBool = false;
  testGame = createGame(".\\savegames\\testsave.txt");
  Logger::logStream(DebugStartup) << "Passed\n";
  //Logger::logStream(DebugStartup) << "Test: ... ";
  //Logger::logStream(DebugStartup) << "Passed\n";  
  Logger::logStream(DebugStartup) << "All tests passed\n.";
}

void WarfareGame::updateGreatestMilStrength() {
  int largest = 0;
  for (MilUnit::Iterator m = MilUnit::begin(); m != MilUnit::end(); ++m) {
    largest = max(largest, (*m)->getTotalStrength());
  }
  for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) {
    Village* village = (*hex)->getVillage();
    if (!village) continue;
    largest = max(largest, village->getMilitia()->getTotalStrength());
  }
  MilStrength::greatestStrength = largest; 
}
