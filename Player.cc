#include "Player.hh"
#include "RiderGame.hh"
#include "Hex.hh" 
#include <algorithm>
#include <cassert> 
#include "Action.hh"
#include "UtilityFunctions.hh" 
#include "PopUnit.hh"
#include "MilUnit.hh" 
#include "Logger.hh" 
#include <queue>

bool detailDebug = false;

Player* Player::currentPlayer      = 0;
double Player::influenceDecay      = 0.5;
double Player::castleWeight        = 1000;
double Player::casualtyValue       = 100; 
double Player::distanceModifier    = 1;
double Player::distancePower       = 1.5; 
double Player::supplyWeight        = 1000;
double Player::siegeInfluenceValue = 20;

Player::Player (bool h, std::string d, std::string n)
  : Iterable<Player>(this)
  , Named<Player>(n, this)
  , EconActor()
  , human(h)
  , doneWithTurn(false)
  , displayName(d)
{}

Player::~Player () {}

double Player::calculateUnitStrength (MilUnit* dat, double modifiers) {
  return dat->calcStrength(dat->getDecayConstant() * modifiers, &MilUnitElement::shock);
}

double Player::calculateInfluence () {
  for (Vertex::Iterator vex = Vertex::start(); vex != Vertex::final(); ++vex) {
    (*vex)->getMirror()->value.influenceMap[this] = 0; 
  }
  double influence = 0; 
  std::queue<Vertex*> infVerts;
  for (Vertex::Iterator v = Vertex::start(); v != Vertex::final(); ++v) {
    Vertex* vex = (*v)->getMirror();
    if (0 == vex->numUnits()) continue;
    MilUnit* unit = vex->getUnit(0);
    if (this != unit->getOwner()) continue;
    double str = calculateUnitStrength(unit, 1.0);
    vex->value.influenceMap[this] = str;
    infVerts.push(vex);
    influence += str*vex->value.strategic;
    //Logger::logStream(DebugAI) << "{" << (*v)->getName() << " " << str*vex->value.strategic << " = " << str << "*" << vex->value.strategic << "} "; 
  }

  for (Line::Iterator l = Line::start(); l != Line::final(); ++l) {
    Line* lin = (*l)->getMirror();
    if (0 == lin->getCastle()) continue;
    if (this != lin->getCastle()->getOwner()) continue;
    int nGar = lin->getCastle()->numGarrison();
    if (0 == nGar) continue;
    double castleInf = 0;     
    for (int i = 0; i < nGar; ++i) {
      castleInf += calculateUnitStrength(lin->getCastle()->getGarrison(i), 1.0 / Castle::getSiegeMod());
    }

    for (int i = 0; i < 2; ++i) {
      Vertex* vex = lin->getVtx(i);
      if ((0 < vex->numUnits()) && (this != vex->getUnit(0)->getOwner())) continue;
      if (vex->value.influenceMap[this] > castleInf) continue; 
      vex->value.influenceMap[this] = castleInf;
      influence += castleInf*vex->value.strategic;
      infVerts.push(vex);
    }
  }

  while (0 < infVerts.size()) {
    Vertex* curr = infVerts.front();
    infVerts.pop();
    double spread = curr->value.influence * influenceDecay; 
    if (10 > spread) continue;
    for (Hex::LineIterator lin = curr->beginLines(); lin != curr->endLines(); ++lin) {
      if (((*lin)->getCastle()) && (this != (*lin)->getCastle()->getOwner())) continue; 
      Vertex* vex = (*lin)->getOther(curr);
      if (vex->value.influenceMap[this] >= spread) continue;
      if ((0 < vex->numUnits()) && (this != vex->getUnit(0)->getOwner())) continue;
      influence -= vex->value.influenceMap[this]*vex->value.strategic;
      influence += spread*vex->value.strategic;
      vex->value.influenceMap[this] = spread;
      infVerts.push(vex); 
    }
  }
  
  return influence; 
}

void Player::clear () {
  Iterable<Player>::clear();
  Named<Player>::clear(); 
}

double Player::evaluateGlobalStrength () {
  
  double ret = 0; 
  double influence = calculateInfluence();

  // Military strength - fighting power of units
  double unitStrength = 0;
  for (Vertex::Iterator v = Vertex::start(); v != Vertex::final(); ++v) {
    Vertex* vex = (*v)->getMirror();
    if (0 == vex->numUnits()) continue;
    MilUnit* unit = vex->getUnit(0);
    if (this == unit->getOwner()) {
      ret -= distanceModifier*pow(vex->value.distanceMap[this], distancePower);
      unitStrength += calculateUnitStrength(unit, 1.0); 
    }
  }

  // Economic and geographic strength - castles, population, productive capacity 
  double castlePoints = 0;
  for (Line::Iterator l = Line::start(); l != Line::final(); ++l) {
    Line* lin = (*l)->getMirror();
    Castle* castle = lin->getCastle();
    if (0 == castle) continue;
    if (this != castle->getOwner()) continue;
    castlePoints += lin->value.strategic * castleWeight;
    
    int nGar = castle->numGarrison(); 
    for (int i = 0; i < nGar; ++i) {
      MilUnit* unit = castle->getGarrison(i);
      unitStrength += calculateUnitStrength(unit, 1.0 / Castle::getSiegeMod());
    }

    Hex* support = castle->getSupport();
    assert(support);
    Village* village = support->getVillage();
    if (!village) continue;
  }
  
  ret += influence;
  ret += castlePoints;
  ret += unitStrength;

  return ret; 
}

double Player::evaluateAttackStrength (Player* att, Player* def) {
  double ret = 0;
  double casualties = 0; 
  att->calculateInfluence();

  // Returns casualties that would be inflicted by att on def
  // if each unit made all the attacks it can, assuming Neutral rolls;
  // plus a bonus for having influence close to enemy-held areas. 
  
  // Attacks on fortresses and sorties out of them
  for (Line::Iterator l = Line::start(); l != Line::final(); ++l) {
    Line* lin = (*l)->getMirror(); 
    Castle* curr = lin->getCastle();
    if (!curr) continue;
    if (def == curr->getOwner()) {
      for (int i = 0; i < 2; ++i) {
	Vertex* vtx = lin->getVtx(i);
	ret += siegeInfluenceValue*vtx->value.influenceMap[att];
	
	MilUnit* unit = vtx->getUnit(0);
	if (!unit) continue;
	if (unit->getOwner() != att) continue; 
	
	MilUnit* defender = curr->getGarrison(0);
	if (!defender) continue;
	casualties += unit->totalSoldiers() * defender->calcBattleCasualties(unit);
      }
    }
    else if ((att == curr->getOwner()) && (0 < curr->numGarrison())) {
      MilUnit* attacker = curr->removeGarrison(); // Remove also cancels fortification bonus. 

      for (int i = 0; i < 2; ++i) {
	Vertex* vtx = lin->getVtx(i);
	
	MilUnit* unit = vtx->getUnit(0);
	if (!unit) continue;
	if (unit->getOwner() != def) continue; 
	casualties += attacker->totalSoldiers() * unit->calcBattleCasualties(attacker); 
      }
      curr->addGarrison(attacker); 
    }
  }

  
  for (Vertex::Iterator vex = Vertex::start(); vex != Vertex::final(); ++vex) {
    Vertex* vtx = (*vex)->getMirror();
    if (0 == vtx->numUnits()) continue;
    MilUnit* mil = vtx->getUnit(0);
    if (att != mil->getOwner()) continue;
    ret += vtx->value.strategic / vtx->value.distanceMap[def];

    for (Vertex::NeighbourIterator n = vtx->beginNeighbours(); n != vtx->endNeighbours(); ++n) {
      if (!(*n)) continue;
      MilUnit* defender = (*n)->getUnit(0);
      if (!defender) continue;
      if (def != defender->getOwner()) continue;
      casualties += mil->totalSoldiers() * defender->calcBattleCasualties(mil);
    }

    for (Vertex::HexIterator h = vtx->beginHexes(); h != vtx->endHexes(); ++h) {
      if (!(*h)) continue;
      if (def != (*h)->getOwner()) continue;
      Village* village = (*h)->getVillage();
      if (!village) continue;
      MilUnit* defenders = village->raiseMilitia(); 
      casualties += mil->totalSoldiers() * defenders->calcBattleCasualties(mil);
    }
  } 

  ret += casualtyValue * casualties; 
  return ret;
}

double Player::evaluate (Action act) { 
  act.player = this; 
  if (Action::Ok != act.checkPossible()) return -100;

  for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) {
    (*hex)->setMirrorState(); 
  }
  for (Vertex::Iterator vex = Vertex::start(); vex != Vertex::final(); ++vex) {
    (*vex)->setMirrorState(); 
  }
  for (Line::Iterator lin = Line::start(); lin != Line::final(); ++lin) {
    (*lin)->setMirrorState(); 
  }

  double ret = 0;

  //Logger::logStream(DebugAI) << "Evaluating " << act.describe() << " ";
  
  for (int i = Disaster; i < NumOutcomes; ++i) {
    double weight = act.probability((Outcome) i); 
    if (weight < 0.00001) continue;
    act.makeHypothetical();
    
    act.forceOutcome((Outcome) i);
    act.execute(); 
    //Action::ActionResult res = act.execute();
    double temp = 0; 
   

    //detailDebug = true;
    temp += evaluateGlobalStrength();
    //detailDebug = false; 

    //Logger::logStream(DebugAI) << "(" << i << " " << temp << " ";
    
    for (Iter p = start(); p != final(); ++p) {
      if ((*p) == this) continue;
      temp -= evaluateAttackStrength((*p), this);
      //Logger::logStream(DebugAI) << temp << " ";
      temp += evaluateAttackStrength(this, (*p));
      //Logger::logStream(DebugAI) << temp << " ";
    }
    
    //Logger::logStream(DebugAI) << res << " " << weight << ") ";
    
    ret += temp*weight;
    act.undo(); // Restore local situation for next loop iteration 
  }

  //Logger::logStream(DebugAI) << "\n";

  Logger::logStream(DebugAI) << "Points from " << act.describe() << " : " << ret << "\n"; 
  return ret; 
}

void recursiveStrategicValue (Vertex* vex, Line* source, double mult, double decay) {
  // Adds strategic value to those neighbours of vex that aren't
  // across source, then repeats with those vertices as the source. 

  if (mult < 1) return;
  assert(decay < 1); 
  
  for (Hex::LineIterator l = vex->beginLines(); l != vex->endLines(); ++l) {
    if ((*l) == source) continue;
    Vertex* other = (*l)->getOther(vex);
    other->value.strategic *= mult;
    recursiveStrategicValue(other, (*l), mult*decay, decay); 
  }
}

void Player::getAction () { 
  std::vector<Action> candidates;
  Action best;
  best.todo = Action::Nothing;
  
  double maxPopulation = 1; 
  for (Vertex::Iterator vex = Vertex::start(); vex != Vertex::final(); ++vex) {
    (*vex)->getMirror()->value.clearFully();
  }
  for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) {
    (*hex)->setMirrorState();
    if ((*hex)->getVillage()) maxPopulation = std::max(maxPopulation, (*hex)->getVillage()->production()); 
  }
  for (Vertex::Iterator vex = Vertex::start(); vex != Vertex::final(); ++vex) {
    (*vex)->setMirrorState(); 
  }
  for (Line::Iterator lin = Line::start(); lin != Line::final(); ++lin) {
    (*lin)->setMirrorState();
    (*lin)->getMirror()->value.clearFully(); 
  }
 
  std::queue<Vertex*> castleDistances; 
  for (Line::Iterator lin = Line::start(); lin != Line::final(); ++lin) {
    Line* mir = (*lin)->getMirror();
    mir->value.strategic *= 3; 
    if (0 == mir->getCastle()) continue;
    for (int i = 0; i < 2; ++i) {
      Vertex* vex = mir->getVtx(i);
      vex->value.distanceMap[mir->getCastle()->getOwner()] = 1;
      castleDistances.push(vex);
      recursiveStrategicValue(vex, mir, 3, 0.5); 
    }
  }

  while (0 < castleDistances.size()) {
    Vertex* curr = castleDistances.front();
    castleDistances.pop();
    for (std::map<Player*, int>::iterator p = curr->value.distanceMap.begin(); p != curr->value.distanceMap.end(); ++p) {
      int newVal = (*p).second + 1;
      for (Vertex::NeighbourIterator vex = curr->beginNeighbours(); vex != curr->endNeighbours(); ++vex) {
	if (!(*vex)) continue;
	bool pushed = false; 
	int oldVal = (*vex)->value.distanceMap[(*p).first];
	//Logger::logStream(DebugAI) << (*p).first->getName() << " " << curr->getName() << " " << (*vex)->getName() << " " << newVal << " " << oldVal << "\n"; 
	if ((0 < oldVal) && (oldVal <= newVal)) continue;
	(*vex)->value.distanceMap[(*p).first] = newVal;
	if (!pushed) {
	  castleDistances.push(*vex);
	  pushed = true;
	}
      }
    }
  }

  for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) {
    Village* village = (*hex)->getVillage();
    double economicWeight = 0;
    if (!village) continue;
    economicWeight = (1 + village->production()) / maxPopulation;
    for (Hex::VtxIterator vex = (*hex)->vexBegin(); vex != (*hex)->vexEnd(); ++vex) {
      (*vex)->value.strategic *= economicWeight; 
    }
  }

  
  double bestScore = evaluate(best); 

  for (Vertex::Iterator vex = Vertex::start(); vex != Vertex::final(); ++vex) {
    if (0 == (*vex)->numUnits()) continue;
    MilUnit* unit = (*vex)->getUnit(0);
    if (unit->getOwner() != this) continue; 
    for (Vertex::NeighbourIterator ngb = (*vex)->beginNeighbours(); ngb != (*vex)->endNeighbours(); ++ngb) {
      Action curr; 
      curr.start = (*vex);
      curr.todo = Action::Attack; 
      curr.final = (*ngb);
      double currScore = evaluate(curr);
      if (currScore < bestScore) continue;
      best = curr;
      bestScore = currScore; 
    }

    for (Hex::LineIterator lin = (*vex)->beginLines(); lin != (*vex)->endLines(); ++lin) {
      Action curr; 
      curr.start = (*vex);
      curr.cease = (*lin);
      if ((*lin)->getCastle()) {
	// If friendly, we might enter the garrison; if not, call for its surrender.
	if ((*lin)->getCastle()->getOwner() == this) {
	  curr.todo = Action::Attack; 
	}
	else {
	  curr.todo = Action::CallForSurrender; 
	}
	double currScore = evaluate(curr);
	if (currScore < bestScore) continue;
	best = curr;
	bestScore = currScore; 
      }
      else {
	// Possible construction.
	curr.todo = Action::BuildFortress;
	curr.source = (*lin)->oneHex();
	double currScore = evaluate(curr);
	if (currScore > bestScore) {
	  best = curr;
	  bestScore = currScore; 
	}
	curr.source = (*lin)->twoHex();
	currScore = evaluate(curr);
	if (currScore > bestScore) {
	  best = curr;
	  bestScore = currScore; 
	}
      }
    }

    for (Vertex::HexIterator hex = (*vex)->beginHexes(); hex != (*vex)->endHexes(); ++hex) {
      if (!(*hex)) continue;
      Action curr;
      curr.start = (*vex);
      curr.target = (*hex);
      curr.todo = Action::Devastate;
      double currScore = evaluate(curr);
      if (currScore < bestScore) continue;
      best = curr;
      bestScore = currScore; 
    }
  }

  for (Line::Iterator lin = Line::start(); lin != Line::final(); ++lin) {
    Castle* castle = (*lin)->getCastle();
    if (!castle) continue;
    if (castle->getOwner() != this) continue;
    Action curr;
    curr.begin = (*lin);
    curr.todo = Action::Recruit;
    curr.cease = (*lin);

    double currScore = 0;
    for (MilUnitTemplate::Iterator ut = MilUnitTemplate::begin(); ut != MilUnitTemplate::end(); ++ut) {
      curr.unitType = (*ut);
      currScore = evaluate(curr);
      //Logger::logStream(DebugAI) << "Score from " << (*ut)->name << " : " << currScore << "\n"; 
      if (currScore > bestScore) {
	best = curr;
	bestScore = currScore;
      }
    }

    Action mob1;
    mob1.todo = Action::Mobilise;
    mob1.begin = (*lin);  // Otherwise begin becomes the mirror, and causes problems. 
    mob1.final = (*lin)->oneEnd();
    currScore = evaluate(mob1);
    if (currScore > bestScore) {
      best = mob1;
      bestScore = currScore;
    }
    Action mob2;
    mob2.todo = Action::Mobilise;
    mob2.begin = (*lin);  
    mob2.final = (*lin)->twoEnd();
    currScore = evaluate(mob2);
    if (currScore > bestScore) {
      best = mob2;
      bestScore = currScore;
    }
  }

  /*
  for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) {
    Action curr;
    curr.todo = Action::Repair;
    curr.target = (*hex);
    double currScore = evaluate(curr);
    if (currScore > bestScore) {
      best = curr;
      bestScore = currScore;
    }        
  }
  */

  best.print = true;
  best.player = this; 
  Logger::logStream(DebugAI) << "Executing " << best.describe() << " " << bestScore << "\n";

  //if (!(best.todo == Action::Devastate)) best.execute();
  best.execute(); 
  finished(); 
}

Player* Player::nextPlayer () {
  Iter pl = std::find(start(), final(), currentPlayer);
  assert(final() != pl);
  do {
    ++pl;
    if (final() == pl) pl = start();
  } while ((*pl)->turnEnded());
  return (*pl); 
}

