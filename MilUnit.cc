#include "MilUnit.hh"
#include "StructUtils.hh"
#include <algorithm>
#include <cmath> 
#include "Logger.hh" 
#include "Calendar.hh"
#include "Player.hh"

typedef std::vector<MilUnitElement*>::const_iterator CElmIter;
typedef std::vector<MilUnitElement*>::iterator ElmIter;
typedef std::vector<MilUnitElement*>::reverse_iterator RElmIter;
typedef std::vector<MilUnitElement*>::const_reverse_iterator CRElmIter; 

vector<double> MilUnit::priorityLevels; 
double MilUnit::defaultDecayConstant = 1000; 
vector<double> MilUnitTemplate::drillEffects;

const double FORAGE_CASUALTY_RATE = 0.01;
const double FORAGE_LOOT_RATE = 0.1;
const double FORAGE_DEFENDER_LOSS_RATE = 0.25;

Unit::Unit ()
  : location(0)
  , rear(Left)
{}

MilUnit::MilUnit ()
  : Unit()
  , Mirrorable<MilUnit>()
  , Named<MilUnit, false>()
  , Iterable<MilUnit>(this)
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
  , graphicsInfo(0)
{}

MilUnit::~MilUnit () {
  for (std::vector<MilUnitElement*>::iterator f = forces.begin(); f != forces.end(); ++f) {
    (*f)->destroyIfReal();
  }
}

MilUnitTemplate::MilUnitTemplate (string n)
  : Enumerable<MilUnitTemplate>(this, n)
{}

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
    MilUnitElement* nElm = new MilUnitElement(temp);
    nElm->soldiers->addPop(str); 
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

MilUnitElement::MilUnitElement (MilUnitTemplate const* const mut)
  : Mirrorable<MilUnitElement>()
  , unitType(mut)
{
  soldiers = new AgeTracker();
  mirror->soldiers = soldiers->getMirror();
  supply = mut->supplyLevels.begin();
}

MilUnitElement::MilUnitElement (MilUnitElement* other)
  : Mirrorable<MilUnitElement>(other)
  , unitType(other->unitType)
{}

MilUnitElement::~MilUnitElement () {
  soldiers->destroyIfReal(); 
}

void MilUnitElement::reCalculate () {
  shock   = unitType->base_shock   * (*supply).fightingModifier;
  range   = unitType->base_range   * (*supply).fightingModifier;
  defense = unitType->base_defense * (*supply).fightingModifier;
  tacmob  = unitType->base_tacmob  * (*supply).movementModifier;
}

void MilUnitElement::setMirrorState () {
  mirror->shock      = shock;
  mirror->range      = range;
  mirror->defense    = defense;
  mirror->tacmob     = tacmob;
  mirror->unitType   = unitType;
  soldiers->setMirrorState();
  mirror->soldiers   = soldiers->getMirror();
  mirror->supply     = supply;
}

void MilUnit::setLocation (Vertex* dat) {
  location = dat;
  leaveMarket();
  if (location) {
    for (Vertex::HexIterator hex = location->beginHexes(); hex != location->endHexes(); ++hex) {
      if ((*hex)->getOwner()->isFriendly(getOwner())) continue;
      (*hex)->registerParticipant(this);
      break;
    }
  }
}

void MilUnit::setMirrorState () {
  mirror->setOwner(getOwner());
  mirror->setRear(getRear());
  mirror->priority = priority;
  mirror->modStack = modStack;
  mirror->aggression = aggression;   
  mirror->fightFraction = fightFraction; 

  mirror->forces.clear();
  for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
    (*i)->setMirrorState();
    mirror->forces.push_back((*i)->getMirror());     
  }
  setEconMirrorState(mirror);
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

  if (0 == forces.size()) return 0;
  if (0 == totalSoldiers()) return 0;
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
  }
  ret *= lifetime;
  return 1 + ret;
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

  forage();
  consumeSupplies();
  recalcElementAttributes();
  graphicsInfo->updateSprites(this);   
}

void MilUnit::consumeSupplies () {
  for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
    (*i)->supply = (*i)->unitType->supplyLevels.end();
  }
  while (true) {
    bool suppliedAny = false;
    for (ElmIter i = forces.begin(); i != forces.end(); ++i) {
      supIter next = (*i)->supply;
      if (next == (*i)->unitType->supplyLevels.end()) next = (*i)->unitType->supplyLevels.begin();
      else ++next;
      if (next == (*i)->unitType->supplyLevels.end()) continue;

      int soldiers = (*i)->strength();
      bool canSupply = true;
      for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
	if (getAmount(*tg) >= soldiers * (*next).getAmount(*tg)) continue;
	canSupply = false;
	break;
      }
      if (!canSupply) continue;
      suppliedAny = true;
      (*i)->supply = next;
      for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
	deliverGoods((*tg), -soldiers*(*next).getAmount(*tg));
      }
    }
    if (!suppliedAny) break;
  }
}

void MilUnit::forage () {
  if (!location) return;
  vector<Hex*> targets;
  for (Vertex::HexIterator hex = location->beginHexes(); hex != location->endHexes(); ++hex) {
    if ((!(*hex)->getVillage()) && (!(*hex)->getFarm()) && (!(*hex)->getForest()) && (!(*hex)->getMine())) continue;
    if ((*hex)->getOwner()->isEnemy(getOwner())) targets.push_back(*hex);
  }

  if (0 == targets.size()) return;
  BOOST_FOREACH(Hex* hex, targets) lootHex(hex);
}

void MilUnit::lootHex (Hex* hex) {
  double forageStrength = getForageStrength();
  vector<MilUnit*> defenders;
  if (hex->getVillage()) defenders.push_back(hex->getVillage()->raiseMilitia());
  Castle* castle = hex->getCastle();
  if (castle) {
    for (int i = 0; i < castle->numGarrison(); ++i) defenders.push_back(castle->getGarrison(i));
  }
  for (Hex::VtxIterator vex = hex->vexBegin(); vex != hex->vexEnd(); ++vex) {
    if ((*vex) == location) continue;
    for (int i = 0; i < (*vex)->numUnits(); ++i) {
      MilUnit* cand = (*vex)->getUnit(i);
      if (cand->getOwner()->isFriendly(hex->getOwner())) defenders.push_back(cand);
    }
  }
  double defenderStrength = 0;
  BOOST_FOREACH(MilUnit* mu, defenders) defenderStrength += mu->getForageStrength();
  double ratio = forageStrength / (1 + defenderStrength);
  // When completely dominant, we get 10% of the available supplies and take no casualties.
  // When utterly outmatched, we get no supplies and take 1% casualties. Move linearly between
  // these two scenarios. Scale everything by our aggression.

  // Scale 0-infinity ratio onto 0-1 using arctan.
  ratio = atan(ratio) * M_2_PI;

  double casualties = takeCasualties((1.0 - ratio) * FORAGE_CASUALTY_RATE * aggression);
  (*this) += hex->loot(FORAGE_LOOT_RATE * ratio * aggression);

  // Defenders take lower casualties because they are assumed
  // to engage only when they have local superiority.
  if (0 == defenders.size()) return;
  casualties *= FORAGE_DEFENDER_LOSS_RATE;
  casualties /= defenders.size();
  BOOST_FOREACH(MilUnit* mu, defenders) {
    double rate = casualties / mu->totalSoldiers();
    mu->takeCasualties(rate);
  }
}

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
    outcome->attackerInfo.fightingFraction = fightFraction;
    outcome->attackerInfo.decayConstant    = getDecayConstant(); 

    outcome->defenderInfo.mobRatio         = 1 - mobRatio;
    outcome->defenderInfo.shock            = thShk;
    outcome->defenderInfo.range            = thFir;
    outcome->defenderInfo.lossRate         = thTotCasRate;
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

void MilUnit::getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {
  list<pair<MilUnitElement const*, supIter> > levels;
  GoodsHolder availableResources(*this);
  GoodsHolder wanted;
  BOOST_FOREACH(MilUnitElement* mue, forces) levels.push_back(pair<MilUnitElement* const, supIter>(mue, mue->unitType->supplyLevels.begin()));
  while (levels.size()) {
    for (list<pair<MilUnitElement const*, supIter> >::iterator level = levels.begin(); level != levels.end(); ++level) {
      bool canSupply = true;
      int soldiers = (*level).first->strength();
      for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
	double amountWanted = soldiers * (*(*level).second).getAmount(*tg);
	if (availableResources.getAmount(*tg) >= amountWanted) continue;
	amountWanted *= prices.getAmount(*tg);
	if (availableResources.getAmount(TradeGood::Money) >= amountWanted) continue;
	canSupply = false;
	break;
      }
      if (canSupply) {
	for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
	  double amountWanted = soldiers * (*(*level).second).getAmount(*tg);
	  if (availableResources.getAmount(*tg) >= amountWanted) availableResources.deliverGoods((*tg), -amountWanted);
	  else {
	    amountWanted *= prices.getAmount(*tg);
	    availableResources.deliverGoods(TradeGood::Money, -amountWanted);
	    wanted.deliverGoods((*tg), amountWanted);
	  }
	}
	++(*level).second;
	if ((*level).second == (*level).first->unitType->supplyLevels.end()) {
	  level = levels.erase(level);
	  if (0 == levels.size()) break;
	  if (level == levels.end()) break;
	}
	continue;
      }

      // We cannot supply this unit further - remove it from the candidates.
      level = levels.erase(level);
      if (0 == levels.size()) break;
      if (level == levels.end()) break;
    }
  }
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    if (0.01 > wanted.getAmount(*tg)) continue;
    bidlist.push_back(new MarketBid((*tg), wanted.getAmount(*tg), this));
  }
}

double MilUnit::getForageStrength () {
  return calcStrength(getDecayConstant(), &MilUnitElement::tacmob) + 0.5*calcStrength(getDecayConstant(), &MilUnitElement::shock);
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
  BOOST_FOREACH(MilUnitElement* mue, forces) mue->reCalculate();
  // TODO: Deal with synergies, whatnot.
}

void MilUnit::setExtMod (double extMod) {
  modStack.push(modStack.size() > 0 ? modStack.top()*extMod : extMod);
}

MilUnitTemplate const* MilUnit::getTestType () {
  static MilUnitTemplate const* unitType = *MilUnitTemplate::start();
  return unitType;
}

MilUnit* MilUnit::getTestUnit () {
  static AgeTracker youngMen;
  if (0 == youngMen.getTotalPopulation()) youngMen.addPop(1000, 16);

  MilUnit* ret = new MilUnit();
  ret->addElement(getTestType(), youngMen);
  return ret;
}

void MilUnit::setPriorityLevels (vector<double> newPs) {
 priorityLevels.clear();
 for (unsigned int p = 0; p < newPs.size(); ++p)
   priorityLevels.push_back(newPs[p]);
 if (0 == priorityLevels.size()) priorityLevels.push_back(1.0); 
}

void MilUnit::unitTests () {
  Player* playerOne = Player::getTestPlayer();
  Player* playerTwo = Player::getTestPlayer();
  MilUnit* testOne = getTestUnit();
  MilUnit* testTwo = getTestUnit();
  MilUnitTemplate const* unitType = getTestType();
  if (2 > unitType->supplyLevels.size()) throwFormatted("Expected %s to have at least 2 supply levels, found %i",
							unitType->getName().c_str(),
							unitType->supplyLevels.size());
  supIter betterSupply = unitType->supplyLevels.begin();
  ++betterSupply;
  testOne->forces.back()->supply = betterSupply;
  testOne->recalcElementAttributes();
  testTwo->recalcElementAttributes();
  double casualtiesOne = testOne->calcBattleCasualties(testTwo);
  double casualtiesTwo = testTwo->calcBattleCasualties(testOne);
  if (casualtiesTwo <= casualtiesOne) throwFormatted("Expected unsupplied unit to take heavier casualties, but found %.2f vs %.2f",
						     casualtiesOne,
						     casualtiesTwo);

  testOne->setAmount(TradeGood::Money, 1000000);
  GoodsHolder prices;
  vector<MarketBid*> bidlist;
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) prices.setAmount((*tg), 1);
  testOne->getBids(prices, bidlist);
  if (0 == bidlist.size()) throwFormatted("Expected unit to bid on goods");
  GoodsHolder actualBids;
  BOOST_FOREACH(MarketBid* mb, bidlist) actualBids.deliverGoods(mb->tradeGood, mb->amountToBuy);
  GoodsHolder expectedBids;
  for (supIter level = unitType->supplyLevels.begin(); level != unitType->supplyLevels.end(); ++level) expectedBids += (*level);
  expectedBids *= 1000;
  double totalExpected = 0;
  double totalActual = 0;
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    double expected = expectedBids.getAmount(*tg);
    double actual = actualBids.getAmount(*tg);
    double diff = expected - actual;
    if (fabs(diff) > 0.01) throwFormatted("Expected to bid for %.2f %s, but found %.2f",
					  expected,
					  (*tg)->getName().c_str(),
					  actual);
    totalExpected += expected;
    totalActual += actual;
  }
  if (0.01 > totalExpected) throwFormatted("Expected total bid should not be zero!");
  if (0.01 > totalActual) throwFormatted("Actual total bid should not be zero!");

  Hex* testHex = Hex::getTestHex();
  Village* testVillage = testHex->getVillage();
  testHex->setOwner(playerTwo);

  TradeGood const* testGood = *(TradeGood::exLaborStart());
  testVillage->setAmount(testGood, 1000);
  testHex->setMirrorState(); // Loot the mirror to avoid side effects of raising militia.
  MilUnit* looter = getTestUnit();
  looter->setOwner(playerOne);
  looter->setAggression(1.0);
  int oldSojers = looter->totalSoldiers();
  looter->lootHex(testHex->getMirror());

  // With no defenders we should take very low casualties and get 10% loot.
  int sojers = looter->totalSoldiers();
  if (fabs(sojers - oldSojers) > 0.005*sojers) throwFormatted("Expected low casualties in looting undefended hex, got %i -> %i, %.2f",
							      oldSojers,
							      sojers,
							      looter->getForageStrength());

  if (fabs(looter->getAmount(testGood) - 1000 * FORAGE_LOOT_RATE) > 1.0) throwFormatted("Expected to loot %.2f %s, but got %.2f",
											1000 * FORAGE_LOOT_RATE,
											testGood->getName().c_str(),
											looter->getAmount(testGood));

  // No defenders and half aggression - half loot.
  looter->zeroGoods();
  looter->setAggression(0.5);
  testVillage->setAmount(testGood, 1000);
  testHex->setMirrorState();
  oldSojers = looter->totalSoldiers();
  looter->lootHex(testHex->getMirror());
  sojers = looter->totalSoldiers();
  if (fabs(sojers - oldSojers) > 0.005*sojers) throwFormatted("Expected low casualties in looting undefended hex again, got %i -> %i, %.2f",
							      oldSojers,
							      sojers,
							      looter->getForageStrength());

  if (fabs(looter->getAmount(testGood) - 1000 * FORAGE_LOOT_RATE * 0.5) > 1.0) throwFormatted("Expected to loot %.2f %s, but got %.2f",
											      1000 * FORAGE_LOOT_RATE * 0.5,
											      testGood->getName().c_str(),
											      looter->getAmount(testGood));
  // Once more, with defense!
  looter->zeroGoods();
  looter->setAggression(1.0);
  MilUnit* defense = getTestUnit();
  defense->setOwner(playerTwo);
  testHex->getVertex(0)->addUnit(defense);
  testVillage->setAmount(testGood, 1000);
  testHex->setMirrorState();
  testHex->getVertex(0)->setMirrorState();
  oldSojers = looter->totalSoldiers();
  looter->lootHex(testHex->getMirror());
  sojers = looter->totalSoldiers();
  if (fabs(sojers - oldSojers) < 0.005*sojers) throwFormatted("Expected some casualties in looting defended hex, got %i -> %i, %.2f vs %.2f",
							      oldSojers,
							      sojers,
							      looter->getForageStrength(),
							      defense->getForageStrength());

  if (fabs(sojers - oldSojers) > 0.01*sojers) throwFormatted("Didn't expect that many casualties in looting defended hex, got %i -> %i, %.2f vs %.2f",
							     oldSojers,
							     sojers,
							     looter->getForageStrength(),
							     defense->getForageStrength());

  if (fabs(looter->getAmount(testGood) - 1000 * FORAGE_LOOT_RATE * 0.5) > 1.0) throwFormatted("Expected to loot %.2f %s, but got %.2f",
											      1000 * FORAGE_LOOT_RATE * 0.5,
											      testGood->getName().c_str(),
											      looter->getAmount(testGood));
  Player::clear();
  delete testHex;
  delete testOne;
  delete testTwo;
  delete looter;
  delete defense;
}

void battleReport (Logger& log, BattleResult& outcome) {
  log << "Strengths: "
      << outcome.attackerInfo.shock << " " << outcome.attackerInfo.range << " : "
      << outcome.attackerInfo.mobRatio   << " : "
      << outcome.defenderInfo.shock << " " << outcome.defenderInfo.range << "\n"
      << "Efficiencies: "
      << outcome.attackerInfo.fightingFraction << " "
      << outcome.attackerInfo.decayConstant << " : "
      << outcome.defenderInfo.fightingFraction << " "
      << outcome.defenderInfo.decayConstant << "\n";
  sprintf(strbuffer, "%.1f%% shock, %.1f%% fire", 100*outcome.shockPercent, 100*outcome.rangePercent);
  log << "Fought with: " << strbuffer << "\n"
      << "Casualties: " << outcome.attackerInfo.casualties << " / " << outcome.defenderInfo.casualties << "\n"
      << "Result: " << (VictoGlory == outcome.attackerSuccess ? "VictoGlory!" : "Failure.") << "\n"; 
}

TransportUnit::TransportUnit ()
  : Unit()
  , EconActor()
  , Iterable<TransportUnit>(this)
  , Mirrorable<TransportUnit>()
{}

TransportUnit::~TransportUnit () {}

TransportUnit::TransportUnit (TransportUnit* other)
  : Unit()
  , EconActor()
  , Iterable<TransportUnit>(0)
  , Mirrorable<TransportUnit>(other)
{}

void TransportUnit::setLocation (Vertex* loc) {
  location = loc;
}

void TransportUnit::setMirrorState () {
  setEconMirrorState(mirror);
}

void TransportUnit::endOfTurn () {
  Vertex* destination = target->getLocation();
  if (!destination) {
    // Indicates that the target has gone into garrison. Ignore for now.
    return;
  }

  vector<Vertex*> route;
  getLocation()->findRoute(route, destination);
  if (0 == route.size()) {
    // Couldn't find a way to get there; ignore.
    return;
  }

  for (int i = 0; i < 3; ++i) {
    route.pop_back(); // Route begins with current vertex, so strip that off.
    if (0 == route.size()) break; // This should never happen.
    setLocation(route.back());
    if (getLocation() != destination) continue;
    target->deliverGoods(*this);
    delete this;
    break;
  }
}
