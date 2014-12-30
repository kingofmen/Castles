#include "MilUnit.hh"
#include "StructUtils.hh"
#include <algorithm>
#include <cmath> 
#include "Logger.hh" 
#include "Calendar.hh"

typedef std::vector<MilUnitElement*>::const_iterator CElmIter;
typedef std::vector<MilUnitElement*>::iterator ElmIter;
typedef std::vector<MilUnitElement*>::reverse_iterator RElmIter;
typedef std::vector<MilUnitElement*>::const_reverse_iterator CRElmIter; 

map<string, MilUnitTemplate const*> MilUnitTemplate::allUnitTemplates;
vector<string> MilUnitTemplate::allUnitTypeNames;
set<MilUnitTemplate const*> MilUnitTemplate::allUnitTypes; 
vector<double> MilUnit::priorityLevels; 
double MilUnit::defaultDecayConstant = 1000; 
vector<double> MilUnitTemplate::drillEffects;

MilUnit::MilUnit ()
  : Unit()
  , Mirrorable<MilUnit>()
  , Named<MilUnit, false>()
  , Iterable<MilUnit>(this)
  , rear(Left)
  , supplies(0)
  , supplyRatio(1) 
  , priority(4)
  , fightFraction(1.0)
  , aggression(0.25)
{
  graphicsInfo = new MilUnitGraphicsInfo(this);
}

MilUnit::MilUnit (MilUnit* other) 
  : Unit()
  , Mirrorable<MilUnit>(other)
  , Named<MilUnit, false>()
  , Iterable<MilUnit>(0)
  , rear(other->rear)
  , graphicsInfo(0)
{}

MilUnit::~MilUnit () {
  for (std::vector<MilUnitElement*>::iterator f = forces.begin(); f != forces.end(); ++f) {
    (*f)->destroyIfReal();
  }
}

MilUnitTemplate::MilUnitTemplate (string n)
  : name(n)
{
  // TODO: Insert check for previous use of name here
  allUnitTemplates[name] = this;
  allUnitTypeNames.push_back(name);
  allUnitTypes.insert(this);

  Logger::logStream(DebugStartup) << "Created template " << this << " " << name << "\n"; 
}

void MilUnit::addElement (MilUnitTemplate const* const temp, AgeTracker& str) {
  assert(temp); 

  //Logger::logStream(Logger::Debug) << (isReal() ? "Real " : "Mirror") << " looking for unit type " << temp->name << " " << str << " " << age << "\n"; 
  bool merged = false;
  for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
    if ((*i)->unitType != temp) continue;
    (*i)->soldiers->addPop(str);
    //Logger::logStream(Logger::Debug) << "Found, new strength " << (*i)->strength() << "\n";
    merged = true;
    break; 
  }

  if (!merged) {
    //Logger::logStream(Logger::Debug) << "Not found, making new\n"; 
    MilUnitElement* nElm = new MilUnitElement();
    nElm->soldiers->addPop(str); 
    nElm->unitType = temp; 
    forces.push_back(nElm);
  }
  recalcElementAttributes(); 
}

//void MilUnit::clear () {
//  for (ElmIter i = forces.begin(); i != forces.end(); ++i) (*i)->strength = 0; 
//}

void MilUnit::demobilise (AgeTracker& target) {
  for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
    for (int j = 0; j < AgeTracker::maxAge; ++j) {
      target.addPop((*i)->soldiers->getPop(j), j);
    }
    (*i)->soldiers->clear(); 
  }
}

/*
MilUnit* MilUnit::detach (double fraction) {
  MilUnit* ret = new MilUnit();
  for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
    int transfer = (int) floor((*i)->strength() * fraction + 0.5);
    (*i)->strength -= transfer;
    ret->addElement((*i)->unitType, transfer);
  }
  ret->rear = rear;
  ret->supplies = fraction * supplies;
  supplies -= ret->supplies;
  ret->supplyRatio = supplyRatio;
  ret->priority = priority; 
  recalcElementAttributes(); 
  return ret; 
}
*/
/*
void MilUnit::transfer (MilUnit* target, double fraction) {
  for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
    int transfer = (int) floor((*i)->strength() * fraction + 0.5);
    (*i)->strength -= transfer;
    // Exploit here due to no supplyRatio recalculation. FIXME. 
    target->addElement((*i)->unitType, transfer);
  }
  recalcElementAttributes(); 
}
*/

int MilUnit::getUnitTypeAmount (MilUnitTemplate const* const ut) const {
  for (CElmIter i = forces.begin(); i != forces.end(); ++i) {
    if ((*i)->unitType != ut) continue;
    return (*i)->strength();
  }
  return 0; 
}

MilUnitElement::MilUnitElement ()
  : Mirrorable<MilUnitElement>()
{
  soldiers = new AgeTracker();
  mirror->soldiers = soldiers->getMirror(); 
}

MilUnitElement::MilUnitElement (MilUnitElement* other)
  : Mirrorable<MilUnitElement>(other)
{}

MilUnitElement::~MilUnitElement () {
  soldiers->destroyIfReal(); 
}


void MilUnitElement::setMirrorState () {
  mirror->shock      = shock;
  mirror->range      = range;
  mirror->defense    = defense;
  mirror->tacmob     = tacmob;
  mirror->unitType   = unitType;
  soldiers->setMirrorState();
  mirror->soldiers = soldiers->getMirror();
}

void MilUnit::setMirrorState () {
  mirror->setOwner(getOwner());
  mirror->rear = rear;
  mirror->supplies = supplies;
  mirror->supplyRatio = supplyRatio; 
  mirror->priority = priority;
  mirror->modStack = modStack;
  mirror->aggression = aggression;   
  mirror->fightFraction = fightFraction; 

  mirror->forces.clear();
  for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
    (*i)->setMirrorState();
    mirror->forces.push_back((*i)->getMirror());     
  }
  mirror->setAmounts(this);  
}

int MilUnit::totalSoldiers () const {
  int ret = 0;
  for (CElmIter i = forces.begin(); i != forces.end(); ++i) {
    ret += (*i)->strength();
  }
  return ret; 
}

double MilUnit::effectiveMobility (MilUnit* const versus) {
  // Suppose I have 1000 men and you have 100. Then I can
  // split off my 100 fastest and send them to pin you down.
  // So my effective speed is that of my 100th fastest man.
  // You, on the other hand, can't afford to split up. So
  // your effective speed is that of your slowest.
  // If the numbers were more even, though, you might split up
  // even if you were outnumbered. 
  // So the effective speed of an army is equal to the Nth fastest,
  // where N is half the number of opposing soldiers.

  int enemyNumber = versus->totalSoldiers(); 
  enemyNumber /= 2;
  sort(forces.begin(), forces.end(), deref<MilUnitElement>(member_lt(&MilUnitElement::tacmob)));

  int count = 0; 
  for (CElmIter i = forces.begin(); i != forces.end(); ++i) {
    count += (*i)->strength();
    if (count > enemyNumber) return (*i)->tacmob;
  }

  return forces.back()->tacmob; 
}

double MilUnit::calcStrength (double lifetime, double MilUnitElement::*field) {
  // Strength increases asymptotically in numbers, with the strongest
  // units contributing first.
  // Higher lifetime is better.

  double gamma = 1.0 / lifetime; 
  
  sort(forces.begin(), forces.end(), deref<MilUnitElement>(member_lt(field)));
  double totalStrength = 0;
  double ret = 0;
  for (CElmIter i = forces.begin(); i != forces.end(); ++i) {
    double curr = (*i)->*field;
    double nums = (*i)->strength() * fightFraction;
    if (1 > nums) continue;
    ret += curr * (exp(-gamma*totalStrength) - exp(-gamma*(totalStrength + nums)));
    totalStrength += nums;
    //Logger::logStream(Logger::Debug) << "Strength after " << nums << " " << (*i)->unitType->name << " : " << totalStrength << " " << ret << "\n"; 
  }
  ret *= lifetime; 
  return 1 + ret; 
}

double MilUnit::getSuppliesNeeded () const {
  double needed = 0.001;
  for (CElmIter i = forces.begin(); i != forces.end(); ++i) {
    needed += (*i)->strength() * (*i)->unitType->supplyConsumption;
  }
  return needed; 
}

double MilUnit::getPrioritisedSuppliesNeeded () const {
  double ret = getSuppliesNeeded();
  ret *= getPriority();
  if (ret < supplies) return 0;
  return ret - supplies;
}

void MilUnit::endOfTurn () {
  if (0 == forces.size()) return; 
  if (0 == totalSoldiers()) return; 

  if (Calendar::Winter == Calendar::getCurrentSeason()) {
    for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
      (*i)->soldiers->age();
    }
    return; 
  }
  
  double needed = getSuppliesNeeded(); 
  supplyRatio = (supplies / needed);
  if (supplyRatio > 1) supplyRatio = sqrt(supplyRatio);
  if (supplyRatio > 3) supplyRatio = 3;
  supplies -= needed*supplyRatio;
  recalcElementAttributes();
  graphicsInfo->updateSprites(this);   
}

const double invHalfPi = 0.6366197730950255;

int MilUnit::takeCasualties (double rate) {
  rate *= fightFraction; 

  int ret = 0; 
  if (rate > 0.9) {
    // Unit disbands from the trauma.
    ret = totalSoldiers(); 
    for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
      delete (*i);
    }
    forces.clear(); 
  }
  else {
    for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
      int loss = 1 + (int) floor((*i)->strength() * rate * 0.01 * (100 - (*i)->defense) + 0.5);
      (*i)->soldiers->die(loss);
      ret += loss;
    }
    recalcElementAttributes(); 
  }

  return ret; 
}

void MilUnit::getShockRange (double shkRatio, double firRatio, double mobRatio, double& shkPercent, double& firPercent) const {
  // Three options: Shock, range, evade. 
  // Armies will try to evade if their enemy would beat them badly, unless
  // they have been ordered to attack. 
  // Otherwise they will fight with the weapon in which their margin
  // of superiority is greatest, with some advantage to shock as that
  // is more deadly.
  if (shkRatio > 0.8*firRatio) { // This army prefers shock.
    if (shkRatio > (1 - aggression)) shkPercent += mobRatio; // Consent to fight shock if we're reasonably even. 
  }
  else { // This army is better at range.
    if (firRatio > (1 - aggression)) firPercent += mobRatio; // We still evade if we'd be beaten. 
  }
}

double MilUnit::calcBattleCasualties (MilUnit* const adversary, BattleResult* outcome) {
  if (0 == adversary->forces.size()) return 0; 
  
  double myMob = effectiveMobility(adversary);
  double myShk = calcStrength(getDecayConstant(), &MilUnitElement::shock);
  double myFir = calcStrength(getDecayConstant(), &MilUnitElement::range); 
  double thMob = adversary->effectiveMobility(this); 
  double thShk = adversary->calcStrength(adversary->getDecayConstant(), &MilUnitElement::shock);
  double thFir = adversary->calcStrength(adversary->getDecayConstant(), &MilUnitElement::range);

  double mobRatio = (myMob < thMob ? pow(myMob / (thMob + myMob), 2) : 1.0 - pow(thMob / (thMob + myMob), 2));
  double shkRatio = thShk / myShk;
  double firRatio = thFir / myFir; 

  double shkPercentage = 0;
  double firPercentage = 0; 
  
  this->getShockRange(1.0/shkRatio, (myShk > 10*myFir ? 0 : 1.0/firRatio), mobRatio, shkPercentage, firPercentage);
  adversary->getShockRange(shkRatio, (thShk > 10*thFir ? 0 : firRatio), (1.0 - mobRatio), shkPercentage, firPercentage);  

  double mySoldiers = totalSoldiers(); 
  double myShkCasRate = 0.03 * thShk * min(1.0, shkRatio);  
  double myFirCasRate = 0.01 * thFir * min(1.0, firRatio);    
  double myTotCasRate  = myShkCasRate * shkPercentage;
  myTotCasRate        += myFirCasRate * firPercentage;
  myTotCasRate         = min(myTotCasRate / mySoldiers, 0.15);

  if (outcome) {
    double thSoldiers = adversary->totalSoldiers(); 
    double thShkCasRate = 0.03 * myShk * min(1.0, 1.0/shkRatio);  
    double thFirCasRate = 0.01 * myFir * min(1.0, 1.0/firRatio);    
    double thTotCasRate  = thShkCasRate * shkPercentage;
    thTotCasRate        += thFirCasRate * firPercentage;
    thTotCasRate         = min(thTotCasRate / thSoldiers, 0.15);
    
    outcome->shockPercent          = shkPercentage;
    outcome->rangePercent          = firPercentage;

    outcome->attackerInfo.mobRatio         = mobRatio;
    outcome->attackerInfo.shock            = myShk;
    outcome->attackerInfo.range            = myFir;
    outcome->attackerInfo.lossRate         = myTotCasRate;
    outcome->attackerInfo.efficiency       = efficiency();
    outcome->attackerInfo.fightingFraction = fightFraction;
    outcome->attackerInfo.decayConstant    = getDecayConstant(); 

    outcome->defenderInfo.mobRatio         = 1 - mobRatio;
    outcome->defenderInfo.shock            = thShk;
    outcome->defenderInfo.range            = thFir;
    outcome->defenderInfo.lossRate         = thTotCasRate;
    outcome->defenderInfo.efficiency       = adversary->efficiency();
    outcome->defenderInfo.fightingFraction = adversary->fightFraction;
    outcome->defenderInfo.decayConstant    = adversary->getDecayConstant(); 
    
  }
  
  return myTotCasRate;   
}

double MilUnit::calcRoutCasualties (MilUnit* const adversary) {
  double ratio = std::max(1.0, calcStrength(getDecayConstant(), &MilUnitElement::tacmob) - adversary->calcStrength(adversary->getDecayConstant(), &MilUnitElement::range));
  ratio /= std::max(1.0, adversary->calcStrength(getDecayConstant(), &MilUnitElement::tacmob) - calcStrength(adversary->getDecayConstant(), &MilUnitElement::range));
  return 0.2*exp(0.1*ratio); 
}

BattleResult MilUnit::attack (MilUnit* const adversary, Outcome dieroll) {
  // Returns VictoGlory if we make the defenders run,
  // Neutral if both armies maintain their positions,
  // Disaster if the attackers are routed. 

  BattleResult ret;
  ret.dieRoll = dieroll;
  ret.attackerSuccess = VictoGlory; 
  
  if (0 == adversary->forces.size()) return ret; 
  
  switch (dieroll) {
  case VictoGlory: setExtMod(3.0); break;
  case Good: setExtMod(2.0); break;
  case Bad: setExtMod(0.75); break;
  case Disaster: setExtMod(0.5); break;
  case Neutral:
  default:
    setExtMod(1.0); 
    break;
  }

  double storeAggression = aggression;
  aggression = 0.99;
  double myLoss = calcBattleCasualties(adversary, &ret);
  double thLoss = ret.defenderInfo.lossRate; 
  aggression = storeAggression;
 
  ret.attackerInfo.casualties = takeCasualties(ret.attackerInfo.lossRate);
  ret.defenderInfo.casualties = adversary->takeCasualties(ret.defenderInfo.lossRate);

  dropExtMod();

  ret.attackerSuccess = Neutral;
  // Rout occurs if one side takes more than 10% casualties.
  if (myLoss > 0.1) ret.attackerSuccess = Disaster;
  if (thLoss > 0.1) ret.attackerSuccess = VictoGlory;
  return ret; 
}

void MilUnit::battleCasualties (MilUnit* const adversary) {
  takeCasualties(calcBattleCasualties(adversary)); 
}

void MilUnit::routCasualties (MilUnit* const adversary) {
  takeCasualties(calcRoutCasualties(adversary)); 
}

int MilUnit::getScoutingModifier (MilUnit* const adversary) {
  double ratio = std::max(1.0, calcStrength(getDecayConstant(), &MilUnitElement::tacmob) - adversary->calcStrength(adversary->getDecayConstant(), &MilUnitElement::range));
  ratio /= std::max(1.0, adversary->calcStrength(getDecayConstant(), &MilUnitElement::tacmob) - calcStrength(adversary->getDecayConstant(), &MilUnitElement::range));

  if (ratio > 3.000) return 10;
  if (ratio > 2.000) return 5;
  if (ratio < 0.500) return -5;
  if (ratio < 0.333) return -10;
  return 0;
}

int MilUnit::getSkirmishModifier (MilUnit* const adversary) {
  double ratio = std::max(1.0, calcStrength(getDecayConstant(), &MilUnitElement::range) - adversary->calcStrength(adversary->getDecayConstant(), &MilUnitElement::defense));
  ratio /= std::max(1.0, adversary->calcStrength(getDecayConstant(), &MilUnitElement::range) - calcStrength(adversary->getDecayConstant(), &MilUnitElement::defense));

  if (ratio > 3.000) return 10;
  if (ratio > 2.000) return 5;
  if (ratio < 0.500) return -5;
  if (ratio < 0.333) return -10;
  return 0; 
}

int MilUnit::getFightingModifier (MilUnit* const adversary) {
  double ratio = std::max(1.0, calcStrength(getDecayConstant(), &MilUnitElement::shock) - adversary->calcStrength(adversary->getDecayConstant(), &MilUnitElement::defense));
  ratio /= std::max(1.0, adversary->calcStrength(getDecayConstant(), &MilUnitElement::shock) - calcStrength(adversary->getDecayConstant(), &MilUnitElement::defense));

  if (ratio > 3.000) return 10;
  if (ratio > 2.000) return 5;
  if (ratio < 0.500) return -5;
  if (ratio < 0.333) return -10;
  return 0; 
}

void MilUnit::recalcElementAttributes () {
  // TODO: Deal with synergies, whatnot.
  for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
    (*i)->shock = (*i)->unitType->base_shock * efficiency();
    (*i)->range = (*i)->unitType->base_range * efficiency();
    (*i)->defense = (*i)->unitType->base_defense * efficiency();
    (*i)->tacmob = (*i)->unitType->base_tacmob * efficiency();
  }
}

void MilUnit::setExtMod (double extMod) {
  modStack.push(modStack.size() > 0 ? modStack.top()*extMod : extMod);
}


void MilUnit::setPriorityLevels (vector<double> newPs) {
 priorityLevels.clear();
 for (unsigned int p = 0; p < newPs.size(); ++p)
   priorityLevels.push_back(newPs[p]);
 if (0 == priorityLevels.size()) priorityLevels.push_back(1.0); 
}

void battleReport (Logger& log, BattleResult& outcome) {
  log << "Strengths: "
      << outcome.attackerInfo.shock << " " << outcome.attackerInfo.range << " : "
      << outcome.attackerInfo.mobRatio   << " : "
      << outcome.defenderInfo.shock << " " << outcome.defenderInfo.range << "\n"
      << "Efficiencies: "
      << outcome.attackerInfo.efficiency << " "
      << outcome.attackerInfo.fightingFraction << " "
      << outcome.attackerInfo.decayConstant << " : "
      << outcome.defenderInfo.efficiency << " "
      << outcome.defenderInfo.fightingFraction << " "
      << outcome.defenderInfo.decayConstant << "\n";
  sprintf(strbuffer, "%.1f%% shock, %.1f%% fire", 100*outcome.shockPercent, 100*outcome.rangePercent);
  log << "Fought with: " << strbuffer << "\n"
      << "Casualties: " << outcome.attackerInfo.casualties << " / " << outcome.defenderInfo.casualties << "\n"
      << "Result: " << (VictoGlory == outcome.attackerSuccess ? "VictoGlory!" : "Failure.") << "\n"; 
}
