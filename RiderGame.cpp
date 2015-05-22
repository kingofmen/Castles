#include "RiderGame.hh"
#include <QtCore>
#include <ctype.h>
#include <list>
#include "boost/function.hpp"
#include "boost/bind.hpp"
#include "Market.hh"
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

using namespace boost;

WarfareGame* WarfareGame::currGame = 0; 
bool testingBool = true;

WarfareGame::WarfareGame () {}

template <class T> void destroyAll () {
  vector<T*> toDestroy; // Transfer here to avoid zapping iterator.
  for (typename T::rIter t = T::rstart(); t != T::rfinal(); ++t) toDestroy.push_back(*t);
  BOOST_FOREACH(T* t, toDestroy) t->destroyIfReal();
}

WarfareGame::~WarfareGame () {
  destroyAll<TransportUnit>();
  destroyAll<TradeUnit>();
  Player::clear();
  EconActor::clear();
  Hex::clear();
  Line::clear();
  Vertex::clear();
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
  for (objiter unit = units.begin(); unit != units.end(); ++unit) StaticInitialiser::buildMilUnit(*unit);
  units = game->getValue("transportUnit");
  for (objiter unit = units.begin(); unit != units.end(); ++unit) StaticInitialiser::buildTransportUnit(*unit);
  units = game->getValue("tradeUnit");
  for (objiter unit = units.begin(); unit != units.end(); ++unit) StaticInitialiser::buildTradeUnit(*unit);

  Player::setCurrentPlayerByName(game->safeGetString("currentplayer"));
  assert(Player::getCurrentPlayer());
  updateGreatestMilStrength();
  StaticInitialiser::clearTempMaps();
  return currGame; 
}

void WarfareGame::findUnits (vector<MilUnit*>& ret, Player* p) {
  for (MilUnit::Iterator m = MilUnit::start(); m != MilUnit::final(); ++m) if ((*m)->getOwner() == p) ret.push_back(*m);
}

void WarfareGame::findCastles (vector<Castle*>& ret, Player* p) {
  for (Line::Iterator lin = Line::start(); lin != Line::final(); ++lin) {
    Castle* curr = (*lin)->getCastle();
    if (!curr) continue;
    if (curr->getOwner() != p) continue;
    ret.push_back(curr);
  }
}

void WarfareGame::endOfTurn () {
  updateGreatestMilStrength();
  for (ContractInfo::Iter c = ContractInfo::start(); c != ContractInfo::final(); ++c) (*c)->execute();
  LineGraphicsInfo::endTurn(); 

  for (TradeUnit::Iterator tu = TradeUnit::start(); tu != TradeUnit::final(); ++tu) (*tu)->endOfTurn();
  for (Vertex::Iterator vex = Vertex::start(); vex != Vertex::final(); ++vex) (*vex)->endOfTurn();
  for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) (*hex)->endOfTurn();
  for (Line::Iterator lin = Line::start(); lin != Line::final(); ++lin) (*lin)->endOfTurn();
  // Supply convoys
  for (TransportUnit::Iter tu = TransportUnit::start(); tu != TransportUnit::final(); ++tu) (*tu)->endOfTurn();
  TransportUnit::cleanUp();
  // Trade
  for (TradeUnit::Iter tu = TradeUnit::start(); tu != TradeUnit::final(); ++tu) (*tu)->endOfTurn();
  // Supply consumption, strength calculation
  for (MilUnit::Iterator mil = MilUnit::start(); mil != MilUnit::final(); ++mil) (*mil)->endOfTurn();

  Calendar::newWeekBegins();
  Logger::logStream(Logger::Game) << Calendar::toString() << "\n";
  
  if (Calendar::Winter == Calendar::getCurrentSeason()) {
    // Hex buildings do special things in winter.
    for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) (*hex)->endOfTurn();

    // So do MilUnits.
    for (MilUnit::Iterator mil = MilUnit::start(); mil != MilUnit::final(); ++mil) (*mil)->endOfTurn();

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
  for (MilUnitTemplate::Iterator t = MilUnitTemplate::start(); t != MilUnitTemplate::final(); ++t) {
    MilUnit* u = new MilUnit();
    recruits.clear();
    recruits.addPop((*t)->recruit_speed, 18);
    u->addElement((*t), recruits);
    units1.push_back(u);
    u->setName((*t)->getName());

    u = new MilUnit();
    u->addElement((*t), recruits);
    u->setName((*t)->getName());
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

int tests = 0;
int passed = 0;

void callTestFunction (string testName, function<void()> fcn) {
  Logger::logStream(DebugStartup) << "Test: " << testName << "...";
  ++tests;  
  try {
    fcn();
    Logger::logStream(DebugStartup) << "Passed\n";
    ++passed;
  }
  catch (string problem) {
    Logger::logStream(DebugStartup) << "Failed with error " << problem << "\n";
  }
}

void callTestFunction (string testName, void (*fPtr) ()) {
  callTestFunction(testName, function<void()>(fPtr));
}

void WarfareGame::unitTests (string fname) {
  ofstream writer;
  writer.open("parseroutput.txt");
  setOutputStream(&writer);
  string savename(".\\savegames\\testsave.txt");
  callTestFunction(string("Creating game from file ") + fname, function<void()>(bind(&WarfareGame::createGame, fname)));
  callTestFunction("EconActor", &EconActor::unitTests);
  callTestFunction("Running a turn", function<void()>(bind(&WarfareGame::endOfTurn, WarfareGame::currGame)));
  callTestFunction(string("Writing to file ") + savename, function<void()>(bind(&StaticInitialiser::writeGameToFile, savename)));
  callTestFunction(string("Loading from savegame again ") + fname, function<void()>(bind(&WarfareGame::createGame, savename)));
  delete currGame;
  callTestFunction("Hex",       &Hex::unitTests);
  callTestFunction("Market",    &Market::unitTests);
  callTestFunction("Village",   &Village::unitTests);
  callTestFunction("Farmland",  &Farmland::unitTests);
  callTestFunction("Forest",    &Forest::unitTests);
  callTestFunction("Mine",      &Mine::unitTests);
  callTestFunction("MilUnit",   &MilUnit::unitTests);
  callTestFunction("Castle",    &Castle::unitTests);
  callTestFunction("TradeUnit", &TradeUnit::unitTests);

  Logger::logStream(DebugStartup) << passed << " of " << tests << " tests passed.\n";
}

void WarfareGame::functionalTests (string fname) {
  callTestFunction(string("Creating game from file") + fname, function<void()>(bind(&WarfareGame::createGame, fname)));
  Hex* testHex = Hex::getHex(0, 0);
  vector<double> labourUsed;
  vector<double> labourAvailable;
  vector<double> employment;
  map<TradeGood const*, vector<double> > goodsPrices;
  map<TradeGood const*, double> consumed;
  map<TradeGood const*, double> produced;
  GoodsHolder startingPrices;
  Market* testMarket = testHex->getMarket();
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) startingPrices.setAmount((*tg), testMarket->getPrice(*tg));

  while (Calendar::Winter != Calendar::getCurrentSeason()) {
    try {
      Logger::logStream(DebugStartup) << Calendar::toString() << "\n";
      labourAvailable.push_back(testHex->getVillage()->production());
      for (Vertex::Iterator vex = Vertex::start(); vex != Vertex::final(); ++vex) (*vex)->endOfTurn();
      for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) (*hex)->endOfTurn();
      Logger::logStream(DebugStartup) << testHex->getVillage()->getBidStatus() << "\n";
      labourUsed.push_back(testMarket->getVolume(TradeGood::Labor));
      employment.push_back(labourUsed.back() / labourAvailable.back());
      Logger::logStream(DebugStartup) << "Good\tPrice\tDemand\tVolume\tProduce\tConsume\tAmount\n";
      for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
	goodsPrices[*tg].push_back(testMarket->getPrice(*tg));
	produced[*tg] += testMarket->getProduced(*tg);
	consumed[*tg] += testMarket->getConsumed(*tg);
	Logger::logStream(DebugStartup) << (*tg)->getName()        << "\t"
					<< testMarket->getPrice(*tg)  << "\t"
					<< testMarket->getDemand(*tg) << "\t"
					<< testMarket->getVolume(*tg) << "\t"
					<< testMarket->getProduced(*tg) << "\t"
					<< testMarket->getConsumed(*tg) << "\t"
					<< testHex->getVillage()->getAmount(*tg) << "\t"
					<< "\n";
      }
      Calendar::newWeekBegins();
    }
    catch (string problem) {
      Logger::logStream(DebugStartup) << "Failed with error " << problem << "\n";
      break;
    }
  }

  doublet labourInfo = calcMeanAndSigma(labourUsed);
  Logger::logStream(DebugStartup) << "\nWork done : " << labourInfo.x() << " +- " << labourInfo.y() << "\n";
  labourInfo = calcMeanAndSigma(employment);
  Logger::logStream(DebugStartup) << "Employment: " << labourInfo.x() << " +- " << labourInfo.y() << "\n";
  Logger::logStream(DebugStartup) << "Prices:\n";
  int passed = 0;
  int tests = 0;
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    doublet curr = calcMeanAndSigma(goodsPrices[*tg]);
    Logger::logStream(DebugStartup) << "  " << (*tg)->getName() << ": " << curr.x() << " +- " << curr.y() << "\n";
    if (curr.y() > 0.5*startingPrices.getAmount(*tg)) Logger::logStream(DebugStartup) << "  Fail: Too high standard deviation\n";
    else ++passed;
    if (curr.x() < 0.1*startingPrices.getAmount(*tg)) Logger::logStream(DebugStartup) << "  Fail: Drastic price drop\n";
    else ++passed;
    if (curr.x() > 5*startingPrices.getAmount(*tg)) Logger::logStream(DebugStartup) << "  Fail: Drastic price rise\n";
    else ++passed;
    tests += 3;
  }
  Logger::logStream(DebugStartup) << "Production and consumption:\n";
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    Logger::logStream(DebugStartup) << "  " << (*tg)->getName() << ": " << produced[*tg] << ", " << consumed[*tg] << "\n";
  }

  Logger::logStream(DebugStartup) << passed << " of " << tests << " functional tests passed.\n";
}

void WarfareGame::updateGreatestMilStrength() {
  int largest = 0;
  for (MilUnit::Iterator m = MilUnit::start(); m != MilUnit::final(); ++m) {
    largest = max(largest, (*m)->getTotalStrength());
  }
  for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) {
    Village* village = (*hex)->getVillage();
    if (!village) continue;
    largest = max(largest, village->getMilitia()->getTotalStrength());
  }
  MilStrength::greatestStrength = largest; 
}
