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

Action::Calculator* Action::attackCalculator    = new Action::Calculator();
Action::Calculator* Action::mobiliseCalculator  = new Action::Calculator();
Action::Calculator* Action::devastateCalculator = new Action::Calculator();
Action::Calculator* Action::surrenderCalculator = new Action::Calculator();
Action::Calculator* Action::recruitCalculator   = new Action::Calculator();
Action::Calculator* Action::colonyCalculator    = new Action::Calculator();
Action::Calculator* Action::successCalculator   = new Action::Calculator();
// Note that this just creates a Calculator without dice, which is
// interpreted as always succeeding - that is, return 'Neutral'.
// The other calculators are initialised by StaticInitialiser. 


const Action::ThingsToDo Action::EndTurn         (Action::successCalculator,   &Action::noop,      &Action::alwaysPossible, "End turn");
const Action::ThingsToDo Action::Colonise        (Action::successCalculator,   &Action::noop,      &Action::alwaysPossible, "Colonise");
const Action::ThingsToDo Action::Attack          (Action::attackCalculator,    &Action::attack,    &Action::canAttack,      "Attack");
const Action::ThingsToDo Action::Mobilise        (Action::mobiliseCalculator,  &Action::mobilise,  &Action::canMobilise,    "Mobilise");
const Action::ThingsToDo Action::BuildFortress   (Action::colonyCalculator,    &Action::build,     &Action::canBuild,       "Build");
const Action::ThingsToDo Action::Devastate       (Action::devastateCalculator, &Action::devastate, &Action::canDevastate,   "Devastate");
const Action::ThingsToDo Action::Reinforce       (Action::successCalculator,   &Action::reinforce, &Action::canReinforce,   "Reinforce"); 
const Action::ThingsToDo Action::CallForSurrender(Action::surrenderCalculator, &Action::surrender, &Action::canSurrender,   "Call for surrender");
const Action::ThingsToDo Action::Recruit         (Action::recruitCalculator,   &Action::recruit,   &Action::canRecruit,     "Recruit"); 
//const Action::ThingsToDo Action::Repair          (Action::successCalculator,   &Action::repair,    &Action::canRepair,      "Repair"); 
const Action::ThingsToDo Action::Nothing         (Action::successCalculator,   &Action::noop,      &Action::alwaysPossible, "Nothing");
const Action::ThingsToDo Action::EnterGarrison   (Action::successCalculator,   &Action::garrison,  &Action::canEnter,       "Enter garrison");


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
  , unitType(0) 
  , print(true)
  , force(NumOutcomes)
  , result(Neutral)
  , activeRear(NoVertex)
  , forcedRear(NoVertex)
  , todo(Nothing)
{
}

Action::ActionResult Action::canAttack () {
  if (!start) return Fail;
  if (!final) return Fail;
  if (0 == start->numUnits()) return NotEnoughPops;
  if (player != start->getUnit(0)->getOwner()) return WrongPlayer;
  if (NoVertex == start->getDirection(final)) return NotAdjacent;
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
  if (NoDirection == source->getDirection(cease)) return NotAdjacent; 
  if (cease->getCastle()) return Impassable;
  bool already = false; 
  for (Hex::LineIterator lin = source->linBegin(); lin != source->linEnd(); ++lin) {
    if (!(*lin)->getCastle()) continue;
    if (source != (*lin)->getCastle()->getSupport()) continue;
    already = true;
    break;
  }
  if (already) return Impassable; 
  //if (8 < source->getDevastation()) return AttackFails; 
  return Ok; 
}
Action::ActionResult Action::canDevastate () {
  if (!start) return Fail;
  if (!target)  return Fail;
  if (0 == start->numUnits()) return NotEnoughPops; 
  if (player != start->getUnit(0)->getOwner()) return WrongPlayer; 
  if (player == target->getOwner()) return WrongPlayer;
  if (!target->getFarm()) return NoBuilding;
  return Ok; 
}
Action::ActionResult Action::canReinforce () {
  if (!start) return Fail;
  if (!final) return Fail;
  if (0 == start->numUnits()) return NotEnoughPops; 
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

  // Old check for overstuffedness - may be reinstated using new MilUnit strength. 
  //if (Castle::maxGarrison <= begin->getCastle()->numGarrison()) return NotEnoughPops;

  // Check for siege - maybe improve with strength? 
  if ((0 < begin->oneEnd()->numUnits()) && (player != begin->oneEnd()->getUnit(0)->getOwner()) &&
      (0 < begin->twoEnd()->numUnits()) && (player != begin->twoEnd()->getUnit(0)->getOwner())) return Impassable;

  
  return Ok; 
}
/*Action::ActionResult Action::canRepair () {
  if (!target) return Fail; 
  if (0 >= target->getDevastation()) return NoBuilding; 
  return Ok; 
}
*/
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
    MilUnit* attacker = start->getUnit(0);
    MilUnit* defender = final->getUnit(0);
    if (start->isReal()) {
      Logger::logStream(Logger::Game) << "Attack by\n"
				      << attacker->getGraphicsInfo()->strengthString("  ")
				      << "  versus\n"
				      << defender->getGraphicsInfo()->strengthString("  ");
    }
    BattleResult result = attacker->attack(defender, out);
    if (start->isReal()) {
      battleReport(Logger::logStream(Logger::Game), result);
    }
    return Ok; 
  }
  
  MilUnit* unit = start->removeUnit();
  final->addUnit(unit);
  activeRear = unit->getRear(); 
  unit->setRear(final->getDirection(start));
  return Ok; 
}

Action::ActionResult Action::mobilise (Outcome out) {
  MilUnit* sortie = begin->getCastle()->removeGarrison();
  
  if (0 < final->numUnits()) {
    MilUnit* unit = final->getUnit(0);
    BattleResult runAway = sortie->attack(unit);
    if (VictoGlory != runAway.attackerSuccess) {
      // Sortie failed to make the besiegers retreat.
      begin->getCastle()->addGarrison(sortie);
      return AttackFails; 
    }
    
    forcedRear = unit->getRear(); 
    Castle* dummyc = 0;
    Vertex* dummyv = 0;
    
    final->forceRetreat(dummyc, dummyv); 
    if (dummyv) unit->setRear(dummyv->getDirection(final)); 
  }
  final->addUnit(sortie);
  final->getUnit(0)->setRear(final->getDirection(begin->getOther(final))); 
  return Ok; 
}

Action::ActionResult Action::build (Outcome out) {
  if (!source->colonise(cease, start->getUnit(0), out)) return AttackFails; 
  return Ok;
}

Action::ActionResult Action::devastate (Outcome out) {
  target->raid(start->getUnit(0), out);  
  return Ok;
}

Action::ActionResult Action::reinforce (Outcome out) {
  if (VictoGlory != out) return Fail;
  return Ok; 
}

Action::ActionResult Action::noop (Outcome out) {
  return Ok;
}

Action::ActionResult Action::surrender (Outcome out) {
  Castle* castle = cease->getCastle();
  castle->callForSurrender(start->getUnit(0), out); 
  return Ok; 
}

Action::ActionResult Action::recruit (Outcome out) {
  switch (out) {
  case Neutral:
    if (unitType) begin->getCastle()->setRecruitType(unitType); 
    begin->getCastle()->recruit(out);
    return Ok;
  default: break;
  }
  return Fail; 
}

void Action::undo () {
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

Action::ActionResult Action::execute () {
  Action::ActionResult ret = checkPossible();
  if (Ok != ret) return ret;

  Outcome result = Neutral; 
  if (todo.calc->die) {
    int dieroll = todo.calc->die->roll();

    result = Disaster; 
    for (int i = VictoGlory; i > Disaster; --i) {
      if (dieroll < todo.calc->results[i]) continue;
      result = (Outcome) i;
      break;
    }
  }

  if (print) {
    Logger::logStream(Logger::Game) << describe() << " "
				    << outcomeToString(result)  
				    << "\n";
  }

  
  if (NumOutcomes != force) result = force;
  ret = (this->*(todo.exec))(result);
  return ret; 
}

double Action::probability (Outcome dis) {
  if (!todo.calc->die) return (Neutral == dis ? 1.0 : 0.0); 

  if (VictoGlory == dis) return todo.calc->die->probability(todo.calc->results[VictoGlory], 0, GtEqual);
  double probDisOrBetter = todo.calc->die->probability(todo.calc->results[dis], 0, GtEqual);
  double probPlainBetter = todo.calc->die->probability(todo.calc->results[dis + 1], 0, GtEqual);
  return (probDisOrBetter - probPlainBetter);
}

Action::ThingsToDo::ThingsToDo (Action::Calculator* p, ActionResult (Action::*e) (Outcome), ActionResult (Action::*c) (), std::string n)
  : calc(p)
  , exec(e)
  , check(c)
  , name(n)
{}

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
{
  results = new int[NumOutcomes];
  for (int i = 0; i < NumOutcomes; ++i) results[i] = 0; 
  
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
