#include "Player.hh"
#include "RiderGame.hh"
#include "Hex.hh" 
#include <algorithm>
#include <cassert> 
#include "Action.hh"
#include "PopUnit.hh" 
#include "Logger.hh" 
#include <queue>

std::vector<Player*> Player::allPlayers; 
bool detail = false; 

Player::Player (bool h, std::string d, std::string n)
  : human(h)
  , doneWithTurn(false)
  , name(n)
  , displayName(d)
{
  allPlayers.push_back(this); 
}

void Player::clear () {
  for (Iterator p = begin(); p != end(); ++p) {
    delete (*p); 
  }
  allPlayers.clear(); 
}

double Player::calculateInfluence () {
  for (Vertex::Iterator vex = Vertex::begin(); vex != Vertex::end(); ++vex) {
    (*vex)->getMirror()->value.influenceMap[this] = 0; 
  }
  double influence = 0; 
  std::queue<Vertex*> infVerts;
  for (Vertex::Iterator v = Vertex::begin(); v != Vertex::end(); ++v) {
    Vertex* vex = (*v)->getMirror();
    if (0 == vex->numUnits()) continue;
    MilUnit* unit = vex->getUnit(0);
    if (this != unit->getOwner()) continue;
    if (unit->weakened()) continue;
    vex->value.influenceMap[this] = 5;
    infVerts.push(vex);
    influence += 5*vex->value.strategic;
  }

  for (Line::Iterator l = Line::begin(); l != Line::end(); ++l) {
    Line* lin = (*l)->getMirror();
    if (0 == lin->getCastle()) continue;
    if (this != lin->getCastle()->getOwner()) continue;
    double castleInf = -1; 
    int nGar = lin->getCastle()->numGarrison();
    switch (nGar) {
    case 0:
    default:
      break;
    case 2: 
    case 1:
      castleInf = 4;
      break;
    }

    if (0 >= castleInf) continue;
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
    double spread = curr->value.influence-1; 
    if (0.1 > spread) continue;
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

double Player::evaluateGlobalStrength (WarfareGame* dat) {
  
  double ret = 0; 
  double influence = calculateInfluence(); 
  double numUnits = 0; 
  for (Vertex::Iterator v = Vertex::begin(); v != Vertex::end(); ++v) {
    Vertex* vex = (*v)->getMirror();
    if (0 == vex->numUnits()) continue;
    MilUnit* unit = vex->getUnit(0);
    if (this == unit->getOwner()) {
      numUnits += (unit->weakened() ? 0.5 : 1.0); 
      ret -= pow(vex->value.distanceMap[this], 1.5); 
    }
  }

  double castlePoints = 0;
  double numCastles = 0;
  for (Line::Iterator l = Line::begin(); l != Line::end(); ++l) {
    Line* lin = (*l)->getMirror();
    if (0 == lin->getCastle()) continue;
    if (this != lin->getCastle()->getOwner()) continue;
    castlePoints += lin->value.strategic * 14;
    numCastles++;
    /*
    Logger::logStream(Logger::Debug) << "Strategic value: "
				     << lin->getName() << " "
				     << lin->value.strategic
				     << "\n";
    */
    numUnits += lin->getCastle()->numGarrison(); 
    numUnits += 0.2*lin->getCastle()->getRecruitState(); 

  }

  if (0 < numCastles) castlePoints /= sqrt(numCastles);
  
  if (detail) Logger::logStream(Logger::Debug) << " ("
					       << influence << " " 
					       << castlePoints << " "
					       << (numUnits < numCastles ? - pow(numCastles - numUnits, 1.5) : 0) << " " 
					       << sqrt(numUnits) << ") "; 
  
  ret += influence;
  ret += castlePoints;
  ret += sqrt(numUnits); 
  if (numUnits < numCastles) ret -= pow(numCastles - numUnits, 1.5);
  
  //Logger::logStream(Logger::Debug) << "  Influence: " << influence << "\n"; 
  return ret; 
}

double Player::evaluateAttackStrength (WarfareGame* dat, Player* att, Player* def) {
  double ret = 0;
  att->calculateInfluence(); 
  for (Line::Iterator l = Line::begin(); l != Line::end(); ++l) {
    Line* lin = (*l)->getMirror(); 
    Castle* curr = lin->getCastle();
    if (!curr) continue;
    if (def != curr->getOwner()) continue;

    Action siege;
    siege.cease = lin; 
    siege.player = att;
    siege.todo = Action::CallForSurrender;
    siege.print = false;
    
    for (int i = 0; i < 2; ++i) {
      Vertex* vtx = lin->getVtx(i);
      siege.start = vtx; 
      double siegeProb = siege.probability(dat, Action::Success);
      Logger::logStream(Logger::Debug) << "[ " << lin->getName() << " " << att->getDisplayName() << " " << siegeProb << " " << vtx->value.influenceMap[att] << " ]"; 
      ret += 20*siegeProb*vtx->value.influenceMap[att]; 
    }
  }
  
  for (Vertex::Iterator vex = Vertex::begin(); vex != Vertex::end(); ++vex) {
    Vertex* vtx = (*vex)->getMirror();
    if (0 == vtx->numUnits()) continue;
    MilUnit* mil = vtx->getUnit(0);
    if (att != mil->getOwner()) continue;
    if (mil->weakened()) continue;
    ret += vtx->value.strategic / vtx->value.distanceMap[def];
    //if (0 == vtx->value.distanceMap[def]) Logger::logStream(Logger::Debug) << "Problem " << vtx->getName() << "\n"; 
    for (Hex::LineIterator lin = vtx->beginLines(); lin != vtx->endLines(); ++lin) {
      Castle* cas = (*lin)->getCastle(); 
      if ((cas) && (def == cas->getOwner())) {
	double castleValue = def->evaluateGlobalStrength(dat);
	(*lin)->addCastle(0);
	castleValue -= def->evaluateGlobalStrength(dat);
	(*lin)->addCastle(cas);
	Action siege;
	siege.cease = (*lin);
	siege.player = att;
	siege.todo = Action::CallForSurrender;
	siege.print = false;
	siege.start = vtx; 
	castleValue *= siege.probability(dat, Action::Success);
	ret += castleValue; 
      }
    }

    for (Vertex::NeighbourIterator n = vtx->beginNeighbours(); n != vtx->endNeighbours(); ++n) {
      if (!(*n)) continue;
      if (0 == (*n)->numUnits()) continue;
      if (def != (*n)->getUnit(0)->getOwner()) continue;
      ret += 2;
      if ((*n)->getUnit(0)->weakened()) ret += 2;
    }

    for (Vertex::HexIterator h = vtx->beginHexes(); h != vtx->endHexes(); ++h) {
      if (!(*h)) continue;
      if (def != (*h)->getOwner()) continue;
      ret += 1; 
    }
  } 
  
  return ret;
}

double Player::evaluate (Action act, WarfareGame* dat) { 
  act.player = this; 
  if (Action::Ok != act.checkPossible(dat)) return -100;

  for (Hex::Iterator hex = Hex::begin(); hex != Hex::end(); ++hex) {
    (*hex)->setMirrorState(); 
  }
  for (Vertex::Iterator vex = Vertex::begin(); vex != Vertex::end(); ++vex) {
    (*vex)->setMirrorState(); 
  }
  for (Line::Iterator lin = Line::begin(); lin != Line::end(); ++lin) {
    (*lin)->setMirrorState(); 
  }

  double ret = 0;
  act.makeHypothetical();
  for (int i = Action::Disaster; i < Action::NumOutcomes; ++i) {
    //for (int i = Action::Success; i >= 0; --i) {
    double weight = act.probability(dat, (Action::Outcome) i); 
    if (weight < 0.00001) continue;
    act.forceOutcome((Action::Outcome) i);
    Action::ActionResult res = act.execute(dat); 

    //detail = true;
    double temp = evaluateGlobalStrength(dat);
    //detail = false; 
    
    Logger::logStream(Logger::Debug) << "Evaluating "
				     << act.describe() << " "
				     << temp << " ";

    for (Iterator p = begin(); p != end(); ++p) {
      if ((*p) == this) continue;
      temp -= evaluateAttackStrength(dat, (*p), this);
      Logger::logStream(Logger::Debug) << temp << " ";
      temp += evaluateAttackStrength(dat, this, (*p));
      Logger::logStream(Logger::Debug) << temp << " ";
    }

    Logger::logStream(Logger::Debug) << i << " "
				     << res << " "
				     << weight << " " 
				     << "\n"; 

    ret += temp*weight;
    act.undo(); 
  }

  return ret; 
}

void Player::getAction (WarfareGame* dat) { 
  std::vector<Action> candidates;
  Action best;
  best.todo = Action::Nothing; 
  double bestScore = 0; 

  for (Vertex::Iterator vex = Vertex::begin(); vex != Vertex::end(); ++vex) {
    (*vex)->getMirror()->value.clearFully();
  }
  for (Hex::Iterator hex = Hex::begin(); hex != Hex::end(); ++hex) {
    (*hex)->setMirrorState(); 
  }
  for (Vertex::Iterator vex = Vertex::begin(); vex != Vertex::end(); ++vex) {
    (*vex)->setMirrorState(); 
  }
  for (Line::Iterator lin = Line::begin(); lin != Line::end(); ++lin) {
    (*lin)->setMirrorState(); 
  }
  
  std::queue<Vertex*> castleDistances; 
  for (Line::Iterator lin = Line::begin(); lin != Line::end(); ++lin) {
    Line* mir = (*lin)->getMirror();
    mir->value.strategic = 1; 
    if (0 == mir->getCastle()) continue;
    for (int i = 0; i < 2; ++i) {
      Vertex* vex = mir->getVtx(i);
      vex->value.distanceMap[mir->getCastle()->getOwner()] = 1;
      //Logger::logStream(Logger::Debug) << vex->getName() << "1\n"; 
      castleDistances.push(vex);
      if ((0 == vex->numUnits()) && (0 < mir->getOther(vex)->numUnits())) vex->value.strategic *= 3; 
	
      for (Hex::LineIterator l = vex->beginLines(); l != vex->endLines(); ++l) {
	if ((*l) == mir) continue;
	Vertex* ve2 = (*l)->getOther(vex);
	for (Hex::LineIterator l2 = ve2->beginLines(); l2 != ve2->endLines(); ++l2) {
	  if ((*l2) == (*l)) continue;
	  (*l2)->value.strategic *= 3;
	  for (int j = 0; j < 2; ++j) {
	    (*l2)->getVtx(j)->value.strategic *= 1.5; 
	  }
	}
      }
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
	//Logger::logStream(Logger::Debug) << (*p).first->getName() << " " << curr->getName() << " " << (*vex)->getName() << " " << newVal << " " << oldVal << "\n"; 
	if ((0 < oldVal) && (oldVal <= newVal)) continue;
	(*vex)->value.distanceMap[(*p).first] = newVal;
	if (!pushed) {
	  castleDistances.push(*vex);
	  pushed = true;
	}
      }
    }
  }
  
  for (Vertex::Iterator vex = Vertex::begin(); vex != Vertex::end(); ++vex) {
    if (0 == (*vex)->numUnits()) continue;
    MilUnit* unit = (*vex)->getUnit(0);
    if (unit->getOwner() != this) continue; 
    for (Vertex::NeighbourIterator ngb = (*vex)->beginNeighbours(); ngb != (*vex)->endNeighbours(); ++ngb) {
      Action curr; 
      curr.start = (*vex);
      curr.todo = Action::Attack; 
      curr.final = (*ngb);
      double currScore = evaluate(curr, dat);
      if (currScore < bestScore) continue;
      best = curr;
      bestScore = currScore; 
    }

    if ((*vex)->getUnit(0)->weakened()) {
      Action curr; 
      curr.start = (*vex);
      curr.final = (*vex);
      curr.todo = Action::Reinforce;
      double currScore = evaluate(curr, dat);
      if (currScore >= bestScore) {
	best = curr;
	bestScore = currScore;
      }
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
	double currScore = evaluate(curr, dat);
	if (currScore < bestScore) continue;
	best = curr;
	bestScore = currScore; 
      }
      else {
	// Possible construction.
	curr.todo = Action::BuildFortress;
	curr.source = (*lin)->oneHex();
	double currScore = evaluate(curr, dat);
	if (currScore > bestScore) {
	  best = curr;
	  bestScore = currScore; 
	}
	curr.source = (*lin)->twoHex();
	currScore = evaluate(curr, dat);
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
      double currScore = evaluate(curr, dat);
      if (currScore < bestScore) continue;
      best = curr;
      bestScore = currScore; 
    }
  }

  for (Line::Iterator lin = Line::begin(); lin != Line::end(); ++lin) {
    if (!(*lin)->getCastle()) continue;
    if ((*lin)->getCastle()->getOwner() != this) continue;
    Action curr;
    curr.begin = (*lin);
    curr.todo = Action::Recruit;
    curr.cease = (*lin);
    double currScore = evaluate(curr, dat);
    if (currScore > bestScore) {
      best = curr;
      bestScore = currScore;
    }    

    Action mob1;
    mob1.todo = Action::Mobilise;
    mob1.begin = (*lin);  // Otherwise begin becomes the mirror, and causes problems. 
    mob1.final = (*lin)->oneEnd();
    currScore = evaluate(mob1, dat);
    if (currScore > bestScore) {
      best = mob1;
      bestScore = currScore;
    }
    Action mob2;
    mob2.todo = Action::Mobilise;
    mob2.begin = (*lin);  
    mob2.final = (*lin)->twoEnd();
    currScore = evaluate(mob2, dat);
    if (currScore > bestScore) {
      best = mob2;
      bestScore = currScore;
    }
  }

  for (Hex::Iterator hex = Hex::begin(); hex != Hex::end(); ++hex) {
    Action curr;
    curr.todo = Action::Repair;
    curr.target = (*hex);
    double currScore = evaluate(curr, dat);
    if (currScore > bestScore) {
      best = curr;
      bestScore = currScore;
    }        
  }

  best.print = true;
  best.player = this; 
  Logger::logStream(Logger::Debug) << "Executing " << best.describe() << " " << bestScore << "\n";
  best.execute(dat);
  finished(); 
}

Player* Player::nextPlayer (Player* curr) {
  Iterator pl = std::find(begin(), end(), curr);
  assert(end() != pl);
  do {
    ++pl;
    if (end() == pl) pl = begin();
  } while ((*pl)->turnEnded());
  return (*pl); 
}

Player* Player::findByName (std::string n) {
  for (Iterator p = begin(); p != end(); ++p) {
    if ((*p)->getName() != n) continue;
    return (*p); 
  }
  return 0; 
}
