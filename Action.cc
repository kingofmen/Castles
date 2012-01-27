#include "Action.hh"
#include "Player.hh"
#include "RiderGame.hh"
#include "Hex.hh"
#include "PopUnit.hh"
#include "MilUnit.hh" 
#include "Logger.hh" 
#include <cassert> 


const DieRoll FiveDSix(5, 6);
const DieRoll FourDSix(4, 6);
const DieRoll ThreeDSix(3, 6); 
const DieRoll TwoDSix(2, 6);
const DieRoll OneDSix(1, 6);
const DieRoll OneDHun(1, 100); 


const Action::ThingsToDo Action::EndTurn         (&Action::alwaysSucceed, &Action::noop,      &Action::alwaysPossible, "End turn");
const Action::ThingsToDo Action::Colonise        (&Action::alwaysSucceed, &Action::noop,      &Action::alwaysPossible, "Colonise");
const Action::ThingsToDo Action::Attack          (&Action::calcAttack,    &Action::attack,    &Action::canAttack,      "Attack");
const Action::ThingsToDo Action::Mobilise        (&Action::calcMobilise,  &Action::mobilise,  &Action::canMobilise,    "Mobilise");
const Action::ThingsToDo Action::BuildFortress   (&Action::alwaysSucceed, &Action::build,     &Action::canBuild,       "Build");
const Action::ThingsToDo Action::Devastate       (&Action::calcDevastate, &Action::devastate, &Action::canDevastate,   "Devastate");
const Action::ThingsToDo Action::Reinforce       (&Action::alwaysSucceed, &Action::reinforce, &Action::canReinforce,   "Reinforce"); 
const Action::ThingsToDo Action::CallForSurrender(&Action::calcSurrender, &Action::surrender, &Action::canSurrender,   "Call for surrender");
const Action::ThingsToDo Action::Recruit         (&Action::calcRecruit,   &Action::recruit,   &Action::canRecruit,     "Recruit"); 
const Action::ThingsToDo Action::Repair          (&Action::alwaysSucceed, &Action::repair,    &Action::canRepair,      "Repair"); 
const Action::ThingsToDo Action::Nothing         (&Action::alwaysSucceed, &Action::noop,      &Action::alwaysPossible, "Nothing");
const Action::ThingsToDo Action::EnterGarrison   (&Action::alwaysSucceed, &Action::garrison,  &Action::canEnter,       "Enter garrison");


Action::Action ()
  : player(0)
  , numUnits(0)
  , source(0)
  , target(0)
  , start(0)
  , final(0)
  , begin(0)
  , cease(0)
  , temporaryUnit(0) 
  , print(true)
  , force(NumOutcomes)
  , result(NoChange)
  , activeRear(Hex::NoVertex)
  , forcedRear(Hex::NoVertex)
  , todo(Nothing)
{
}

Action::ActionResult Action::canAttack () {
  if (!start) return Fail;
  if (!final) return Fail;
  if (0 == start->numUnits()) return NotEnoughPops;
  if (player != start->getUnit(0)->getOwner()) return WrongPlayer;
  if (Hex::NoVertex == start->getDirection(final)) return NotAdjacent;
  if ((0 < final->numUnits()) && (final->getUnit(0)->getOwner() == player)) return Impassable; 
  Line* middle = start->getLine(final);
  if (!middle) return NotAdjacent; 
  if ((middle->getCastle()) && (middle->getCastle()->getOwner() != player)) return Impassable; 
  return Ok;
}

Action::ActionResult Action::canEnter () {
  if (!start) return Fail;
  if (!cease) return Fail;
  if (0 == start->numUnits()) return NotEnoughPops;
  if (player != start->getUnit(0)->getOwner()) return WrongPlayer;
  if ((start != cease->oneEnd()) && (start != cease->twoEnd())) return NotAdjacent; 
  if (!cease->getCastle()) return NoBuilding; 
  if (cease->getCastle()->getOwner() != player) return WrongPlayer;
  if (Castle::maxGarrison <= cease->getCastle()->numGarrison()) return Impassable;
  return Ok;
}

Action::ActionResult Action::canMobilise () {
  if (!begin) return Ok;
  if (!final) return Ok; 
  if (!begin->getCastle()) return NoBuilding;
  if (0 == begin->getCastle()->numGarrison()) return NotEnoughPops;
  if (player != begin->getCastle()->getOwner()) return WrongPlayer;
  if (0 < final->numUnits()) {
    if (final->getUnit(0)->getOwner() == player) return WrongPlayer;
    return Ok;
  }
  return Ok;
}
Action::ActionResult Action::canBuild () {
  if (!cease) return Fail;
  if (!source) return Fail;
  if (!start) return Fail; 
  if (0 == start->numUnits()) return NotEnoughPops; 
  if (Hex::None == source->getDirection(cease)) return NotAdjacent; 
  if (cease->getCastle()) return Impassable;
  bool already = false; 
  for (Hex::LineIterator lin = source->linBegin(); lin != source->linEnd(); ++lin) {
    if (!(*lin)->getCastle()) continue;
    if (source != (*lin)->getCastle()->getSupport()) continue;
    already = true;
    break;
  }
  if (already) return Impassable; 
  if (8 < source->getDevastation()) return AttackFails; 
  return Ok; 
}
Action::ActionResult Action::canDevastate () {
  if (!start) return Fail;
  if (!target)  return Fail;
  if (0 == start->numUnits()) return NotEnoughPops; 
  if (player != start->getUnit(0)->getOwner()) return WrongPlayer; 
  if (player == target->getOwner()) return WrongPlayer; 
  if (start->getUnit(0)->weakened()) return NotEnoughPops; 
  return Ok; 
}
Action::ActionResult Action::canReinforce () {
  if (!start) return Fail;
  if (!final) return Fail;
  if (0 == start->numUnits()) return NotEnoughPops; 
  if (!start->getUnit(0)->weakened()) return NotEnoughPops; 
  if (player != start->getUnit(0)->getOwner()) return WrongPlayer; 
  if (start != final) return NotAdjacent; 
  return Ok; 
}
Action::ActionResult Action::canSurrender () {
  if (!start) return Fail;
  if (!cease) return Fail;
  if ((start != cease->oneEnd()) && (start != cease->twoEnd())) return NotAdjacent; 
  if (!cease->getCastle()) return NoBuilding; 
  if (cease->getCastle()->getOwner() == player) return WrongPlayer; 
  if (0 == start->numUnits()) return NotEnoughPops; 
  if (player != start->getUnit(0)->getOwner()) return NotEnoughPops; 
  return Ok;    
}
Action::ActionResult Action::canRecruit () {
  if (!begin) return Fail;
  if (!cease) return Fail;
  if (begin != cease) return NotAdjacent; 
  if (!begin->getCastle()) return NoBuilding;
  if (begin->getCastle()->getOwner() != player) return WrongPlayer; 
  if (Castle::maxGarrison <= begin->getCastle()->numGarrison()) return NotEnoughPops; 
  if ((0 < begin->oneEnd()->numUnits()) && (player != begin->oneEnd()->getUnit(0)->getOwner()) &&
      (0 < begin->twoEnd()->numUnits()) && (player != begin->twoEnd()->getUnit(0)->getOwner())) return Impassable; 
  return Ok; 
}
Action::ActionResult Action::canRepair () {
  if (!target) return Fail; 
  if (0 >= target->getDevastation()) return NoBuilding; 
  return Ok; 
}

void Action::makeHypothetical () {
  print = false; 
  if (source) {
    source->setMirrorState(); 
    source = source->getMirror();
    assert(source);
  }
  if (target) {
    target->setMirrorState(); 
    target = target->getMirror();
    assert(target); 
  }
  if (start) {
    start->setMirrorState(); 
    start = start->getMirror();
    assert(start);
  }
  if (final) {
    final->setMirrorState(); 
    final = final->getMirror();
    assert(final);
  }
  if (begin) {
    begin->setMirrorState(); 
    begin = begin->getMirror();
    assert(begin);
  }
  if (cease) {
    cease->setMirrorState(); 
    cease = cease->getMirror();
    assert(cease);
  }
}

Action::ActionResult Action::garrison (Outcome out) {
  MilUnit* unit = start->removeUnit();
  cease->getCastle()->addGarrison(unit);
  activeRear = unit->getRear(); 
  return Ok;
}

Action::ActionResult Action::attack (Outcome out) {
  if (0 < final->numUnits()) {
    start->getUnit(0)->battleCasualties(final->getUnit(0));
    final->getUnit(0)->battleCasualties(start->getUnit(0));
    switch (out) {
    case Disaster:
      return AttackFails;      
    case NoChange:
      return AttackFails;      
    case Success:
      {
      MilUnit *unit = final->getUnit(0);
      forcedRear = unit->getRear();
      Castle* dummyc = 0;
      Vertex* dummyv = 0; 
      final->forceRetreat(dummyc, dummyv); 
      if (dummyv) unit->setRear(final->getDirection(dummyv)); 
      break;
      }
    default: return Fail; 
    }
  }
  /*
  Logger::logStream(Logger::Debug) << "Moving unit from "
				   << start->getName() << " to "
				   << final->getName() << "\n";
  */
  MilUnit* unit = start->removeUnit();
  final->addUnit(unit);
  activeRear = unit->getRear(); 
  unit->setRear(final->getDirection(start)); 
  return Ok; 
}

Action::ActionResult Action::mobilise (Outcome out) {
  if (Success != out) return AttackFails; 

  if (0 < final->numUnits()) {
    MilUnit* unit = final->getUnit(0);
    forcedRear = unit->getRear(); 
    Castle* dummyc = 0;
    Vertex* dummyv = 0; 
    final->forceRetreat(dummyc, dummyv); 
    if (dummyv) unit->setRear(final->getDirection(dummyv)); 
  }
  final->addUnit(begin->getCastle()->removeGarrison());
  final->getUnit(0)->setRear(final->getDirection(begin->getOther(final))); 
  return Ok; 
}

Action::ActionResult Action::build (Outcome out) {
  if (Success != out) return Fail; 
  Castle* castle = new Castle(source);
  castle->setOwner(player);
  castle->addGarrison(start->removeUnit());
  addContent(cease, castle, &Line::addCastle); 
  return Ok;
}

Action::ActionResult Action::devastate (Outcome out) {
  switch (out) {
  case Disaster:
    start->getUnit(0)->weaken();
  case NoChange:
    return AttackFails;
  case Success:
    target->raid();
    return Ok;
  default: break;
  }
  return Fail; 
}

Action::ActionResult Action::reinforce (Outcome out) {
  if (Success != out) return Fail;
  start->getUnit(0)->reinforce();
  return Ok; 
}

Action::ActionResult Action::noop (Outcome out) {
  return Ok;
}

Action::ActionResult Action::surrender (Outcome out) {
  switch (out) {
  case Success:
    {
    Castle* castle = cease->getCastle();
    castle->setOwner(player);
    castle->getSupport()->setOwner(player);
    return Ok;
    }
  case Disaster:
    start->getUnit(0)->weaken();
    return AttackFails;
  case NoChange:
    return AttackFails;
  default: break;
  }
  return Fail;
}

Action::ActionResult Action::recruit (Outcome out) {
  switch (out) {
  case Disaster:
  case NoChange:
    return AttackFails;
  case Success:
    temporaryUnit = begin->getCastle()->recruit();
    return Ok;
  default: break;
  }
  return Fail; 
}

Action::ActionResult Action::repair (Outcome out) {
  if (Success != out) return Fail; 
  target->repair();
  return Ok; 
}

void Action::undo () {
  if (NoChange == result) return;
  print = true;
  if (source) {source = source->getReal(); source->setMirrorState();}
  if (target) {target = target->getReal(); target->setMirrorState();}
  if (start)  {start  = start ->getReal(); start->setMirrorState(); }
  if (final)  {final  = final ->getReal(); final->setMirrorState(); }
  if (begin)  {begin  = begin ->getReal(); begin->setMirrorState(); }
  if (cease)  {cease  = cease ->getReal(); cease->setMirrorState(); }
 
  if (temporaryUnit) delete temporaryUnit;
  temporaryUnit = 0; 
}

Action::ActionResult Action::execute (WarfareGame* dat) {
  result = NoChange;
  Action::ActionResult ret = checkPossible(dat);
  if (Ok != ret) return ret;
  Calculator calc = (this->*(todo.calc))();
  if (print) {
    Logger::logStream(Logger::Game) << describe() << "\n";
  }
  if (calc.die) {
    int dieroll = calc.die->roll();
    dieroll += calc.mods;
    if (dieroll >= calc.success) result = Success;
    else if (dieroll <= calc.disaster) result = Disaster;     
    if ((calc.unmodifiedDisaster) && (dieroll - calc.mods <= calc.disaster)) {
      result = Disaster;
      if (print) {
	Logger::logStream(Logger::Game) << "Rolled "
					<< dieroll-calc.mods 
					<< " before modifiers, disaster!\n";
      }
    }
    else if (print) {
      Logger::logStream(Logger::Game) << "Rolled "
				      << dieroll-calc.mods << " + "
				      << calc.mods << " = "
				      << dieroll << ", needed "
				      << calc.success << " for success.\n"; 
    }
  }
  else result = Success;

  if (NumOutcomes != force) result = force;
  ret = (this->*(todo.exec))(result);
  return ret; 
}

double Action::probability (WarfareGame* dat, Outcome dis) {
  Calculator calc = (this->*(todo.calc))();
  if (!calc.die) return (Success == dis ? 1.0 : 0.0); 

  double successProb = calc.die->probability(calc.success, calc.mods, GtEqual);
  double disasterProb = calc.die->probability(calc.disaster, calc.mods, LtEqual);
  if (calc.unmodifiedDisaster) {
    double unmodProb = calc.die->probability(calc.disaster, 0, LtEqual);
    if (unmodProb > disasterProb) disasterProb = unmodProb;
    if (unmodProb + successProb > 1) successProb = 1 - unmodProb; 
  }
  switch (dis) {
  case Success: return successProb;
  case NoChange: return (1 - successProb - disasterProb);
  case Disaster: return disasterProb;
  default:
    return 0; 
  }
}

Action::ThingsToDo::ThingsToDo (Action::Calculator (Action::*p)(), ActionResult (Action::*e) (Outcome), ActionResult (Action::*c) (), std::string n)
  : calc(p)
  , exec(e)
  , check(c)
  , name(n)
{}

Action::Calculator Action::calcAttack () {
  Calculator ret;
  if (0 == final->numUnits()) return ret;
  MilUnit* attacker = start->getUnit(0);
  MilUnit* defender = final->getUnit(0);
  ret.mods += attacker->getScoutingModifier(defender);
  ret.mods += attacker->getSkirmishModifier(defender);
  ret.mods += attacker->getFightingModifier(defender);
  
  ret.die = &OneDHun;
  ret.success = 70;
  ret.disaster = 10;
  ret.unmodifiedDisaster = true; 
  return ret; 
}
Action::Calculator Action::calcMobilise () {
  Calculator ret;
  ret.success = 0;
  if (0 == final->numUnits()) return ret;
  ret.die = &OneDSix;
  ret.success = 7 - begin->getCastle()->numGarrison();
  if (final->getUnit(0)->weakened()) ret.mods = 1; 
  ret.disaster = 0;
  ret.unmodifiedDisaster = false;
  return ret; 
}
Action::Calculator Action::calcDevastate () {
  Calculator ret;
  ret.die = &TwoDSix; 
  int blocks = 0;
  int support = -1; 
  for (Hex::VtxIterator vex = target->vexBegin(); vex != target->vexEnd(); ++vex) {
    if (0 == (*vex)->numUnits()) continue;
    if ((*vex)->getUnit(0)->getOwner() == player) support++;
    else if ((*vex)->getUnit(0)->getOwner() == target->getOwner()) blocks++; 
  }
  
  for (Hex::LineIterator lin = target->linBegin(); lin != target->linEnd(); ++lin) {
    if (!(*lin)->getCastle()) continue;
    if ((*lin)->getCastle()->getOwner() == player) continue;
    blocks += 2;
    if ((0 < (*lin)->oneEnd()->numUnits()) && (player == (*lin)->oneEnd()->getUnit(0)->getOwner())) blocks--;
    if ((0 < (*lin)->twoEnd()->numUnits()) && (player == (*lin)->twoEnd()->getUnit(0)->getOwner())) blocks--; 
  }
  
  ret.mods = support - blocks; 
  ret.disaster = 2;
  ret.success = 10; 
  return ret; 
}

Action::Calculator Action::calcSurrender () {
  Calculator ret;
  ret.die = &TwoDSix;
  ret.success = 12;
  int dev = cease->getCastle()->getSupport()->getDevastation();
  int support = 0;
  int garrison = 0; 
  if ((0 < cease->getOther(start)->numUnits()) && (player == cease->getOther(start)->getUnit(0)->getOwner())) support = 3;
  switch (cease->getCastle()->numGarrison()) {
  case 2:
  default:
    break;
  case 1:
    garrison = 1;
    break;
  case 0:
    garrison = 100;
    break; 
  }
  ret.mods = dev+support+garrison; 
  ret.disaster = 2;
  ret.unmodifiedDisaster = true; 
  return ret; 
}
Action::Calculator Action::calcRecruit () {
  Calculator ret;
  ret.die = &TwoDSix;
  ret.success = begin->getCastle()->getSupport()->getDevastation();
  return ret; 
}

DieRoll::DieRoll (int d, int f) 
 : dice(d)
 , faces(f)
 , next(0)
{
  if (dice > 1) next = new DieRoll(dice-1, f); 
}

double DieRoll::probability (int target, int mods, RollType t) const {
  if (1 == dice) return baseProb(target, mods, t); 
  target -= mods;
  double ret = 0;
  for (int i = 1; i <= faces; ++i) {
    ret += next->probability(target - i, 0, t);
  }
  ret /= faces; 
  return ret; 
}

int DieRoll::roll () const {
  int ret = dice; 
  for (int i = 0; i < dice; ++i) {
    ret += (rand() % faces);
  }
  return ret; 
}


double DieRoll::baseProb (int target, int mods, RollType t) const {
  // Returns probability of roll <operator> target on one die. 
  
  target -= mods; 
  double numTargets = 1.0; 
  switch (t) {
  case Equal:
    if (target < 1) return 0;
    if (target > faces) return 0; 
    return (numTargets / faces);
    
  case GtEqual:
    if (target <= 1) return 1;
    if (target > faces) return 0;
    numTargets = 1 + faces - target; 
    return numTargets/faces; 
    
  case LtEqual:
    if (target >= faces) return 1;
    if (target <= 0) return 0;
    numTargets = target; 
    return numTargets/faces; 
    
  case Greater:
    if (target < 1) return 1;
    if (target >= faces) return 0;
    numTargets = faces - target;
    return numTargets/faces; 
    
  case Less:
    if (target > faces) return 1;
    if (target < 1) return 0;
    numTargets = target - 1; 
    return numTargets/faces; 
    
  default: break; 
  }
  return 0;
}

Action::Calculator::Calculator ()
  : die(0)
  , success(0)
  , disaster(-1)
  , mods(0)
  , unmodifiedDisaster(false)
{}

Action::Calculator Action::alwaysSucceed () {
  Calculator ret;
  return ret; 
}

void buildStr (std::string& str, Named const * const nam, int& idx) {
  if (!nam) return; 
  static std::string adjectives[3] = {" from ", " to ", " via "};
  str += adjectives[idx++];
  str += nam->getName();
}



std::string Action::describe () const {
  std::string ret = (player ? player->getDisplayName() : std::string("Temp"));
  ret += " ";
  ret += todo.name;
  int adjidx = 0;
  buildStr(ret, source, adjidx);
  buildStr(ret, start, adjidx);
  buildStr(ret, begin, adjidx);
  buildStr(ret, cease, adjidx);
  buildStr(ret, final, adjidx);
  buildStr(ret, target, adjidx); 
  return ret; 
}
