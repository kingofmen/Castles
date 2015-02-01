#include "Building.hh"
#include "PopUnit.hh"
#include "Hex.hh"
#include <algorithm> 
#include "MilUnit.hh"
#include "UtilityFunctions.hh"
#include <cassert> 
#include "Calendar.hh" 

int Village::maxPopulation = 1; 
vector<double> Village::baseMaleMortality(AgeTracker::maxAge);
vector<double> Village::baseFemaleMortality(AgeTracker::maxAge);
vector<double> Village::pairChance(AgeTracker::maxAge);
vector<double> Village::fertility(AgeTracker::maxAge);
vector<double> Village::products(AgeTracker::maxAge);
vector<double> Village::consume(AgeTracker::maxAge);
vector<double> Village::recruitChance(AgeTracker::maxAge);
double Village::femaleProduction = 0.5;
double Village::femaleConsumption = 0.90;
double Village::femaleSurplusEffect = -2.0;
double Village::femaleSurplusZero = 1.0;
double Castle::siegeModifier = 10; 
vector<Village::MaslowLevel*> Village::maslowLevels;

int Farmland::_labourToSow    = 1;
int Farmland::_labourToPlow   = 10;
int Farmland::_labourToClear  = 100;
int Farmland::_labourToWeed   = 3;
int Farmland::_labourToReap   = 10;
int Farmland::_cropsFrom3     = 1000;
int Farmland::_cropsFrom2     = 500;
int Farmland::_cropsFrom1     = 100;

int Forest::_labourToTend    = 5;
int Forest::_labourToHarvest = 50;
int Forest::_labourToClear   = 250;
vector<int> Forest::_amountOfWood;

int Mine::_amountOfIron = 50;

char errorMessage[500];

Castle::Castle (Hex* dat, Line* lin) 
  : Mirrorable<Castle>()
  , support(dat)
  , location(lin)
  , recruitType(0) 
{
  recruitType = *(MilUnitTemplate::begin());
  setMirrorState();
}

Castle::Castle (Castle* other)
  : Mirrorable<Castle>(other)
  , support(0) // Sequence issues - this constructor is called before anything is initialised in real constructor
{}

Castle::~Castle () {
  for (std::vector<MilUnit*>::iterator i = garrison.begin(); i != garrison.end(); ++i) {
    (*i)->destroyIfReal();
  }
  garrison.clear();
}

void Castle::addGarrison (MilUnit* p) {
  assert(p); 
  garrison.push_back(p);
  p->setLocation(0);
  p->setExtMod(siegeModifier); // Fortification bonus 
}

void Castle::callForSurrender (MilUnit* siegers, Outcome out) {
  if (0 == garrison.size()) {
    setOwner(siegers->getOwner());
    return;
  }

  BattleResult assault = siegers->attack(garrison[0], out);
  if (VictoGlory == assault.attackerSuccess) {
    garrison[0]->destroyIfReal();
    garrison[0] = garrison.back();
    garrison.pop_back();
    if (0 == garrison.size()) {
      setOwner(siegers->getOwner());
      return;
    }
  }
}

void Castle::endOfTurn () {
  //support->getVillage()->demandSupplies(&taxExtraction);
  //supplies += taxExtraction.delivered;
  //taxExtraction.delivered = 0;
}

void Castle::supplyGarrison () {
  if (0 == garrison.size()) return; 
  double totalRequired = 0.001; 
  for (vector<MilUnit*>::iterator m = garrison.begin(); m != garrison.end(); ++m) {
    totalRequired += (*m)->getPrioritisedSuppliesNeeded();
  }
  double fraction = supplies / totalRequired;
  if (fraction > 1) fraction = 1; 
  for (vector<MilUnit*>::iterator m = garrison.begin(); m != garrison.end(); ++m) {
    double required = (*m)->getPrioritisedSuppliesNeeded();
    required *= fraction;
    supplies -= required;
    (*m)->addSupplies(required); 
  }
}

MilUnit* Castle::removeGarrison () {
 MilUnit* ret = garrison.back();
 garrison.pop_back();
 ret->dropExtMod(); // No longer gets fortification bonus. 
 return ret;
 }

double Castle::removeSupplies (double amount) {
  if (amount > supplies) amount = supplies;
  supplies -= amount;
  return amount; 
}

MilUnit* Castle::removeUnit (MilUnit* dat) {
  std::vector<MilUnit*>::iterator target = std::find(garrison.begin(), garrison.end(), dat);
  if (target == garrison.end()) return 0;
  MilUnit* ret = (*target);
  garrison.erase(target); 
  return ret; 
}

void Castle::recruit (Outcome out) {
  Logger::logStream(DebugStartup) << "Recruiting: " << this << " " << (void*) &recruitType << " " << (void*) recruitType << "\n"; 
  if (!recruitType) recruitType = *(MilUnitTemplate::begin());
  MilUnit* target = (garrison.size() > 0 ? garrison[0] : new MilUnit());
  int newSoldiers = support->recruit(getOwner(), recruitType, target, out);
  if (0 == garrison.size()) {
    if (0 == newSoldiers) delete target;
    else {
      addGarrison(target); 
      target->setOwner(getOwner()); 
    }
  }
}

void Building::setOwner (Player* p) {
  owner = p;
}

void Castle::setOwner (Player* p) {
  Building::setOwner(p);
  if (isReal()) mirror->setOwner(p); 
  for (vector<MilUnit*>::iterator u = garrison.begin(); u != garrison.end(); ++u) {
    (*u)->setOwner(p); 
  }
}

void Castle::setMirrorState () {
  mirror->setOwner(getOwner()); 
  mirror->support = support;
  mirror->location = location; 
  mirror->supplies = supplies;
  mirror->recruitType = recruitType;

  mirror->garrison.clear();  
  for (std::vector<MilUnit*>::iterator unt = garrison.begin(); unt != garrison.end(); ++unt) {
    (*unt)->setMirrorState(); 
    mirror->garrison.push_back((*unt)->getMirror());
  }
  mirror->setAmounts(this);
}

Village::Village ()
  : Building()
  , Mirrorable<Village>()
  , milTrad(0)
  , farm(0)
  , consumptionLevel(maslowLevels[0])
  , workedThisTurn(0)
{
  milTrad = new MilitiaTradition();
}

Village::Village (Village* other)
  : Building()
  , Mirrorable<Village>(other)
  , milTrad(0)
  , farm(0)
  , consumptionLevel(maslowLevels[0])
  , workedThisTurn(0)
{}

Village::~Village () {
  if (milTrad) milTrad->destroyIfReal();
}

void Village::getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {
  // For each level in the Maslow hierarchy,
  // see if we can sell enough labor to cover it
  // (at the given prices). Then put in a sell bid
  // for enough labor to buy the last level we can
  // completely cover, and buy bids to get us that
  // level.

  double moneyAvailable     = getAmount(TradeGood::Money);
  double consumptionFactor  = consumption();
  double laborAvailable     = production();
  if (0.001 > laborAvailable) return;
  double inverseTotalLabour = 1.0 / laborAvailable;
  GoodsHolder amountToBuy;
  GoodsHolder reserveUsed;
  expectedConsumptionLevel = 0;
  stopReason = "max consumption";
  BOOST_FOREACH(MaslowLevel const* level, maslowLevels) {
    double moneyNeeded = 0;
    bool canGetLevel = true;
    double labourNeeded = 0;
    for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
      double amountNeeded = level->getAmount(*tg) * consumptionFactor;
      double reserve = getAmount(*tg) - reserveUsed.getAmount(*tg);
      if (reserve > 0) {
	reserve = min(amountNeeded, reserve);
	amountNeeded -= reserve;
      }
      if (amountNeeded < 0.001) continue;
      if (TradeGood::Labor == (*tg)) {
	// Deal with labour specially since we produce it and presumably don't need to buy.
	if (amountNeeded > laborAvailable) {
	  stopReason = "not enough labour";
	  canGetLevel = false;
	  break;
	}
	else {
	  laborAvailable -= amountNeeded;
	  labourNeeded += amountNeeded;
	  continue;
	}
      }
      moneyNeeded += prices.getAmount(*tg) * amountNeeded;
      if (moneyNeeded > moneyAvailable + laborAvailable*prices.getAmount(TradeGood::Labor)) {
	canGetLevel = false;
	stopReason = (*tg)->getName() + " too expensive";
	break;
      }
      else labourNeeded += moneyNeeded / prices.getAmount(TradeGood::Labor);
    }
    if (labourNeeded * inverseTotalLabour > level->maxWorkFraction) {
      canGetLevel = false;
      stopReason = "too much work";
    }
    
    if (!canGetLevel) break;

    amountToBuy.deliverGoods(TradeGood::Labor, -labourNeeded);
    for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
      double amountNeeded = level->getAmount(*tg) * consumptionFactor;
      double reserve = getAmount(*tg) - reserveUsed.getAmount(*tg);
      if (reserve > 0) {
	reserve = min(amountNeeded, reserve);
	reserveUsed.deliverGoods((*tg), reserve);
	amountNeeded -= reserve;
      }      
      if (amountNeeded < 0.001) continue;
      amountToBuy.deliverGoods((*tg), amountNeeded);
    }
    expectedConsumptionLevel = level;
  }

  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    double amount = amountToBuy.getAmount(*tg);
    if (fabs(amount) < 1) continue;
    bidlist.push_back(new MarketBid((*tg), amount, this));
  }
}

Village* Village::getTestVillage (int pop) {
  Village* testVillage = new Village();
  testVillage->males.addPop(pop, 20);
  return testVillage;
}

void Village::unitTests () {
  Village* testVillage = getTestVillage(1000); // 1000 20-year-old males... wonder what they do for entertainment?
  GoodsHolder prices;
  vector<MarketBid*> bidlist;
  prices.deliverGoods(TradeGood::Labor, 1);
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    prices.deliverGoods((*tg), 0.25);
  }
  testVillage->getBids(prices, bidlist);
  if (0 == bidlist.size()) throw string("Village should have made at least one bid.");
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if ((mb->tradeGood == TradeGood::Labor) && (mb->amountToBuy > 0)) throw string("Should be selling, not buying, labor.");
    else if ((mb->tradeGood != TradeGood::Labor) && (mb->amountToBuy < 0)) throw string("Should be buying, not selling, ") + mb->tradeGood->getName();
  }

  double labor = testVillage->produceForContract(TradeGood::Labor, 100);
  if (fabs(labor - 100) > 0.1) {
    sprintf(errorMessage, "Village should have produced 100 labor, got %f", labor);    
    throw string(errorMessage);
  }
  delete testVillage;

  // Testing that increasing labour prices produce more labour.
  testVillage = getTestVillage(100);
  double oldConsume = consume[20];
  consume[20] = 1;
  vector<MaslowLevel*> backupLevels = maslowLevels;
  maslowLevels.clear();
  maslowLevels.push_back(new MaslowLevel(1.0, 0.501)); // Get a tolerance issue if I use 0.5 exactly; (50 / 100) > 0.5.
  maslowLevels.push_back(new MaslowLevel(1.0, 0.45));
  TradeGood const* theGood = *(TradeGood::exLaborStart());
  maslowLevels[0]->setAmount(theGood, 0.1);
  maslowLevels[1]->setAmount(theGood, 0.1);
  prices.clear();
  prices.setAmount(TradeGood::Labor, 1);
  prices.setAmount(theGood, 5);
  // Buying 0.2 food now costs 1 labour. The village should be unwilling
  // to make that trade because the maximum for the second level is 0.45.
  // It should still buy the first level, however.
  bidlist.clear();
  testVillage->getBids(prices, bidlist);
  if (2 != bidlist.size()) throwFormatted("Expected 2 bids, got %i, reason %s", bidlist.size(), testVillage->stopReason.c_str());
  double labourBought = 0;
  BOOST_FOREACH(MarketBid* mb, bidlist) if (mb->tradeGood == TradeGood::Labor) labourBought += mb->amountToBuy;
  // Minus one from selling
  double labourExpected = -1 * (maslowLevels[0]->getAmount(theGood) * testVillage->consumption() * prices.getAmount(theGood)) / prices.getAmount(TradeGood::Labor); 
  if (0.001 < fabs(labourExpected - labourBought)) throwFormatted("Expected to buy %f labour, but bought %f, reason %s",
								  labourExpected,
								  labourBought,
								  testVillage->stopReason.c_str());

  prices.setAmount(TradeGood::Labor, 2);
  // Now the full trade, both levels, should be accepted.
  bidlist.clear();
  testVillage->getBids(prices, bidlist);
  if (2 != bidlist.size()) throwFormatted("Expected 2 bids (second time), got %i, reason %s", bidlist.size(), testVillage->stopReason.c_str());
  labourBought = 0;
  BOOST_FOREACH(MarketBid* mb, bidlist) if (mb->tradeGood == TradeGood::Labor) labourBought += mb->amountToBuy;
  labourExpected = -1 * ((maslowLevels[0]->getAmount(theGood) + maslowLevels[1]->getAmount(theGood)) * testVillage->consumption() * prices.getAmount(theGood)) / prices.getAmount(TradeGood::Labor); 
  if (0.001 < fabs(labourExpected - labourBought)) throwFormatted("Expected (second time) to buy %f labour, but bought %f, reason %s",
								  labourExpected,
								  labourBought,
								  testVillage->stopReason.c_str());
  consume[20] = oldConsume;

  delete testVillage;
}

string Village::getBidStatus () const {
  return (consumptionLevel ? consumptionLevel->name : "none") + " "
    +    (expectedConsumptionLevel ? expectedConsumptionLevel->name : "none") + " "
    +    stopReason;
}

const MilUnitGraphicsInfo* Village::getMilitiaGraphics () const {
  return milTrad ? milTrad->militia->getGraphicsInfo() : 0;
}

MilitiaTradition::MilitiaTradition ()
  : Mirrorable<MilitiaTradition>()
{
  militia = new MilUnit();
  setMirrorState();
}

MilitiaTradition::~MilitiaTradition () {
  militia->destroyIfReal();
}

MilitiaTradition::MilitiaTradition (MilitiaTradition* other)
  : Mirrorable<MilitiaTradition>(other)
{}

void MilitiaTradition::increaseTradition (MilUnitTemplate const* target) { 
  if (!target) target = getKeyByWeight<MilUnitTemplate const* const>(militiaStrength);
  if (target) militiaStrength[target]++;
}

void MilitiaTradition::decayTradition () {
  for (map<MilUnitTemplate const* const, int>::iterator i = militiaStrength.begin(); i != militiaStrength.end(); ++i) {
    int loss = convertFractionToInt((*i).second * (*i).first->militiaDecay * MilUnitTemplate::getDrillEffect(drillLevel));
    militiaStrength[(*i).first] -= loss;
  }
}

double MilitiaTradition::getRequiredWork () {
  double ret = 0; 
  for (map<MilUnitTemplate const* const, int>::iterator i = militiaStrength.begin(); i != militiaStrength.end(); ++i) {
    ret += drillLevel * (*i).first->recruit_speed * (*i).first->militiaDrill * (*i).second; 
  }
  return ret; 
}

int MilitiaTradition::getUnitTypeAmount (MilUnitTemplate const* const ut) const {
  if (militiaStrength.find(ut) == militiaStrength.end()) return 0; 
  return ut->recruit_speed * militiaStrength.at(ut); // Use 'at' for const-ness. 
}


void MilitiaTradition::setMirrorState () {
  militia->setMirrorState();  
  mirror->militia = militia->getMirror();
  for (map<MilUnitTemplate const* const, int>::iterator i = militiaStrength.begin(); i != militiaStrength.end(); ++i) {
    mirror->militiaStrength[(*i).first] = (*i).second;
  }
  mirror->drillLevel = drillLevel; 
}


double Village::adjustedMortality (int age, bool male) const {
  // Mortality per week. The arrays store per year, so divide by weeks in a year.
  double mortMod = consumptionLevel ? consumptionLevel->mortalityModifier : 1.5;
  if (male) return baseMaleMortality[age] * mortMod * Calendar::inverseYearLength;
  return baseFemaleMortality[age] * mortMod * Calendar::inverseYearLength;
}

double Village::produceForContract (TradeGood const* const tg, double amount) {
  if (amount < 0) throw string("Cannot produce negative amount");
  if (tg == TradeGood::Labor) {
    amount = min(amount, production());
    if (amount < 0) throw string("Cannot work more than production");    
    workedThisTurn += amount;
    return amount;
  }
  return EconActor::produceForContract(tg, amount);
}

void Village::endOfTurn () {
  eatFood();
  workedThisTurn = 0;

  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    double fracLoss = adjustedMortality(i, true) * males.getPop(i);
    males.addPop(-convertFractionToInt(fracLoss), i);
    
    fracLoss = adjustedMortality(i, false) * women.getPop(i);
    women.addPop(-convertFractionToInt(fracLoss), i);
  }
 
  // Simplifying assumptions:
  // No man ever impregnates an older woman
  // Male fertility is constant, only female fertility matters
  // No man gets more than one woman pregnant in a year 
  // Young men get the first chance at single women

  double popIncrease = 0;
  vector<int> takenWomen(AgeTracker::maxAge);
  for (int mAge = 16; mAge < AgeTracker::maxAge; ++mAge) {
    for (int fAge = 16; fAge <= mAge; ++fAge) {
      int availableWomen = women.getPop(fAge) - takenWomen[fAge];
      if (1 > availableWomen) continue; 
      double pairs = min(availableWomen, males.getPop(mAge)) * pairChance[mAge - fAge];
      double pregnancies = pairs * fertility[fAge] * Calendar::inverseYearLength;
      takenWomen[fAge] += (int) floor(pairs);
      popIncrease += convertFractionToInt(pregnancies); 
    }
  }

  males.addPop((int) floor(0.5 * popIncrease + 0.5), 0);
  women.addPop((int) floor(0.5 * popIncrease + 0.5), 0);

  updateMaxPop();

  Calendar::Season currSeason = Calendar::getCurrentSeason();
  if (Calendar::Winter != currSeason) return;
  males.age();
  women.age();

  milTrad->decayTradition(); 

  MilUnitGraphicsInfo* milGraph = (MilUnitGraphicsInfo*) milTrad->militia->getGraphicsInfo();  
  if (milGraph) milGraph->updateSprites(milTrad);   
}

void Village::demobMilitia () {
  if (!milTrad) return;
  milTrad->militia->demobilise(males); 
}

MilUnit* Village::raiseMilitia () {
  for (map<MilUnitTemplate const* const, int>::iterator i = milTrad->militiaStrength.begin(); i != milTrad->militiaStrength.end(); ++i) {
    for (int j = 0; j < (*i).second; ++j) {
      produceRecruits((*i).first, milTrad->militia, Neutral); 
    }
  }

  if (milTrad->isReal()) {
    Logger::logStream(Logger::Game) << "Militia raised: \n"
				    << milTrad->militia->getGraphicsInfo()->strengthString("  ");
  }

  milTrad->increaseTradition();  
  return milTrad->militia; 
}

Farmland::Farmland () 
  : Building(1)
  , Mirrorable<Farmland>()
  , blockSize(5)
{
  for (int j = 0; j < numOwners; ++j) {
    farmers.push_back(new Farmer(this));
  }
  for (int i = 0; i < NumStatus; ++i) {
    totalFields[i] = 0;
  }  
}

Farmland::~Farmland () {
  BOOST_FOREACH(Farmer* farmer, farmers) {
    farmer->destroyIfReal();
  }
}

Farmland::Farmland (Farmland* other)
  : Building(1)
  , Mirrorable<Farmland>(other)
  , blockSize(5)    
{}

Farmland::Farmer::Farmer (Farmland* b)
  : Industry<Farmer>(this)
  , Mirrorable<Farmland::Farmer>()
  , fields(NumStatus, 0)
  , boss(b)
{}

Farmland::Farmer::Farmer (Farmer* other)
  : Industry<Farmer>(this)
  , Mirrorable<Farmland::Farmer>(other)
  , fields(NumStatus, 0)
{}

void Farmland::Farmer::setMirrorState () {
  mirror->owner = owner;
  for (unsigned int i = 0; i < fields.size(); ++i) mirror->fields[i] = fields[i];
  mirror->boss = boss->getMirror();
}

Farmland::Farmer::~Farmer () {}

double Farmland::Farmer::outputOfBlock (int block) const {
  // May be inaccurate for the last block. TODO: It would be good to have
  // an expectation value instead, with a discount rate and accounting for
  // the current state of the block.
  return _cropsFrom3 * boss->blockSize;
}

Farmland* Farmland::getTestFarm (int numFields) {
  Farmland* testFarm = new Farmland();
  testFarm->farmers[0]->fields[Clear] = numFields;
  return testFarm;
}

void Farmland::overrideConstantsForUnitTests (int lts, int ltp, int ltw, int ltr) {
  _labourToSow  = lts;
  _labourToPlow = ltp;
  _labourToWeed = ltw;
  _labourToReap = ltr;
}

void Farmland::unitTests () {
  Farmland* testFarm = getTestFarm();
  // Stupidest imaginable test, but did actually catch a problem, so...
  if ((int) testFarm->farmers.size() != numOwners) {
    sprintf(errorMessage, "Farm should have %i Farmers, has %i", numOwners, testFarm->farmers.size());
    throw string(errorMessage);
  }

  testFarm->farmers[0]->unitTests();
}

int Farmland::Farmer::numBlocks () const {
  int total = 0;
  BOOST_FOREACH(int f, fields) total += f;
  int blocks = total / boss->blockSize;
  if (blocks * boss->blockSize < total) ++blocks;
  return blocks;
}

void Farmland::Farmer::unitTests () {
  if (!output) throw string("Farm output has not been set.");
  if (0 >= boss->blockSize) throw new string("Farmer expected positive block size");
  // Note that this is not static, it's run on a particular Farmer.
  GoodsHolder oldCapital(*capital);
  capital->clear();
  TradeGood const* testCapGood = 0;
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    if ((*tg) == output) continue;
    testCapGood = (*tg);
    capital->setAmount((*tg), 0.1);
    break;
  }

  Calendar::newYearBegins();
  fields[Clear] = 1400;
  double laborNeeded = getLabourForBlock(0);
  if (laborNeeded <= 0) throwFormatted("Expected to need positive amount of labor, but found %f", laborNeeded);

  deliverGoods(testCapGood, 1);
  double laborNeededWithCapital = getLabourForBlock(0);
  if (laborNeededWithCapital >= laborNeeded) throwFormatted("With capital %f of %s, should require less than %f labour, but need %f",
							    getAmount(testCapGood),
							    testCapGood->getName().c_str(),
							    laborNeeded,
							    laborNeededWithCapital);
  deliverGoods(testCapGood, -1);

  // This makes us need 100 labour per Spring turn on 1400 clear fields, if capital is zero.
  // With capital 1, we need 100 * (1 - 0.1 log(2)) = 93. So we are saving
  // 7 labor with 1 iron. At price 1, that's net-present-value of 70. So,
  // there should be a bid for iron if the price is below seventy, but not if
  // it is above.
  int oldLabourToSow = _labourToSow;
  int oldLabourToPlow = _labourToPlow;
  _labourToSow = 0;
  _labourToPlow = 1;
  laborNeeded = 0;
  for (int i = 0; i < numBlocks(); ++i) laborNeeded = getLabourForBlock(i);
  GoodsHolder prices;
  vector<MarketBid*> bidlist; 
  // Expect to produce 1400 * 700 / 42 food per turn, using 100 labour;
  // that's 233.3 output per labour. So price of 210 is 10% profit.
  prices.deliverGoods(TradeGood::Labor, 210);
  prices.deliverGoods(testCapGood, 1);
  prices.deliverGoods(output, 1);
  deliverGoods(output, 100);
  getBids(prices, bidlist);
  
  double foundLabour = 0;
  double foundCapGood = 0;
  double foundOutput = 0;
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) foundLabour += mb->amountToBuy;
    else if (mb->tradeGood == testCapGood) foundCapGood += mb->amountToBuy;
    else if (mb->tradeGood == output) foundOutput += mb->amountToBuy;
    else throwFormatted("Expected to bid on %s and %s and to sell %s, but got bid for %f %s (capital %f price %f) %i %i %f",
			TradeGood::Labor->getName().c_str(),
			testCapGood->getName().c_str(),
			output->getName().c_str(),
			mb->amountToBuy,
			mb->tradeGood->getName().c_str(),
			capital->getAmount(mb->tradeGood),
			prices.getAmount(mb->tradeGood),
			mb->tradeGood->getIdx(),
			testCapGood->getIdx(),
			capital->getAmount(testCapGood));
  }
  if (foundLabour <= 0) throwFormatted("Expected to buy %s, found", TradeGood::Labor->getName().c_str(), foundLabour);
  if (foundCapGood <= 0) throwFormatted("Expected to buy %s,found", testCapGood->getName().c_str(), foundCapGood);
  if (foundOutput >= -1) throwFormatted("Expected to sell at least one %s, found %f", output->getName().c_str(), foundOutput);
  if (foundOutput < -5) throwFormatted("Did not expect to sell more than 5 units, found %f", foundOutput);

  // Reduce the profit, check that we sell less.
  prices.setAmount(TradeGood::Labor, 220);
  bidlist.clear();
  getBids(prices, bidlist);
  double secondFoundOutput = 0;
  foundLabour = 0;
  foundCapGood = 0;
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) foundLabour += mb->amountToBuy;
    else if (mb->tradeGood == testCapGood) foundCapGood += mb->amountToBuy;
    else if (mb->tradeGood == output) foundOutput += mb->amountToBuy;
  }
  if (foundLabour <= 0) throwFormatted("Expected (again) to buy %s, found", TradeGood::Labor->getName().c_str(), foundLabour);
  if (foundCapGood <= 0) throwFormatted("Expected (again) to buy %s,found", testCapGood->getName().c_str(), foundCapGood);
  if (fabs(secondFoundOutput) >= fabs(foundOutput)) throwFormatted("Profit decrease should lead to lower bid offer, found %f -> %f", foundOutput, secondFoundOutput);
  
  deliverGoods(output, -100);

  owner = this;
  int canonicalBlocks = numBlocks();
  while (Calendar::getCurrentSeason() != Calendar::Winter) {
    int currentBlocks = numBlocks();
    if (currentBlocks != canonicalBlocks) {
      sprintf(errorMessage,
	      "Expected to have %i blocks, but have %i. Clear %i, Ready %i, Sowed %i, Ripe1 %i, Ripe2 %i, Ripe3 %i, Ended %i",
	      canonicalBlocks,
	      currentBlocks,
	      fields[Clear],
	      fields[Ready],
	      fields[Sowed],
	      fields[Ripe1],
	      fields[Ripe2],
	      fields[Ripe3],
	      fields[Ended]);
      throw string(errorMessage);
    }
    laborNeeded = 0;    
    for (int i = 0; i < currentBlocks; ++i) laborNeeded += getLabourForBlock(i);
    setAmount(TradeGood::Labor, laborNeeded);
    workFields();
    Calendar::newWeekBegins();
  }
  if (fields[Ended] != 1400) {
    sprintf(errorMessage,
	    "Expected to have 1400 ended fields, but have %i. Clear %i, Ready %i, Sowed %i, Ripe1 %i, Ripe2 %i, Ripe3 %i",
	    fields[Ended],
	    fields[Clear],
	    fields[Ready],
	    fields[Sowed],
	    fields[Ripe1],
	    fields[Ripe2],
	    fields[Ripe3]);
    throw string(errorMessage);
  }
  if (fabs(getAmount(output) - _cropsFrom3*1400) > 1) {
    sprintf(errorMessage, "Expected to have %f %s, but have %f", _cropsFrom3*1400.0, output->getName().c_str(), getAmount(output));
    throw string(errorMessage);
  }

  boss->blockSize = 5;
  fields[Ended] = 6;
  vector<int> theBlock(NumStatus, 0);
  fillBlock(0, theBlock);
  if (theBlock[Ended] != boss->blockSize) {
    sprintf(errorMessage, "Expected %i Ended fields in block 0, got %i", boss->blockSize, theBlock[Ended]);
    throw string(errorMessage);
  }
  theBlock[Ended] = 0;

  fields[Ended] = 100;
  fillBlock(5, theBlock);
  if (theBlock[Ended] != boss->blockSize) {
    sprintf(errorMessage, "Expected %i Ended fields in block 5, got %i", boss->blockSize, theBlock[Ended]);
    throw string(errorMessage);
  }
  theBlock[Ended] = 0;

  fields[Ended] = 1;
  fillBlock(1, theBlock);
  if (theBlock[Ended] != 0) {
    sprintf(errorMessage, "Expected 0 Ended fields in block 1, got %i", theBlock[Ended]);
    throw string(errorMessage);
  }
  theBlock[Ended] = 0;
  
  fields[Ended] = boss->blockSize - 1;
  fields[Ripe3] = 1;
  fillBlock(0, theBlock);
  if (theBlock[Ended] != boss->blockSize - 1) {
    sprintf(errorMessage, "Expected %i Ended fields in block 0, got %i", boss->blockSize - 1, theBlock[Ended]);
    throw string(errorMessage);
  }
  if (theBlock[Ripe3] != 1) {
    sprintf(errorMessage, "Expected 1 Ripe3 fields in block 0, got %i", theBlock[Ripe3]);
    throw string(errorMessage);
  }

  theBlock[Ended] = 0;
  theBlock[Ripe3] = 0;
  fields[Ripe2] = boss->blockSize;
  fillBlock(1, theBlock);
  if (theBlock[Ended] != 0) throw string("Did not expect Ended fields in block 1");
  if (theBlock[Ripe3] != 0) throw string("Did not expect Ripe3 fields in block 1");
  if (theBlock[Ripe2] != boss->blockSize) {
    sprintf(errorMessage, "Expected %i Ripe2 fields in block 1, got %i", boss->blockSize, theBlock[Ripe2]);
    throw string(errorMessage);    
  }

  Calendar::setWeek(30);
  if (Calendar::Autumn != Calendar::getCurrentSeason()) throw string("Expected week 30 to be Autumn");
  for (unsigned int i = 0; i < fields.size(); ++i) fields[i] = 0;
  setAmount(output, 0);
  fields[Ripe3] = boss->blockSize;
  setAmount(TradeGood::Labor, getLabourForBlock(0) * Calendar::turnsToNextSeason());
  workFields();
  double firstOutput = getAmount(output);
  if (0.1 > firstOutput) throw string("Expected output larger than 0.1");
  
  vector<double> marginFactors;
  marginFactors.push_back(0.99);
  marginFactors.push_back(0.9);
  marginFactors.push_back(0.1);
  BOOST_FOREACH(double mf, marginFactors) {
    boss->marginFactor = mf;    
    for (int blocks = 2; blocks < 5; ++blocks) {
      fields[Ended] = 0;
      fields[Ripe3] = blocks*boss->blockSize;
      double gameExpected = 0;
      for (int j = 0; j < blocks; ++j) gameExpected += outputOfBlock(j) * pow(mf, j);
      setAmount(output, 0);
      setAmount(TradeGood::Labor, blocks*getLabourForBlock(0) * Calendar::turnsToNextSeason());
      workFields();
      // Geometric sum, with growth rate r, from 0 to n, is (1-r^(n+1)) / (1-r).
      double coderExpected = firstOutput * (1 - pow(mf, blocks)) / (1 - mf);
      double actual = getAmount(output);
      if (0.1 < fabs(coderExpected - actual)) {
	sprintf(errorMessage,
		"With margin %f and %i blocks, expected %f but got %f",
		mf,
		blocks,
		coderExpected,
		actual);
	throw string(errorMessage);
      }
      if (0.1 < fabs(gameExpected - actual)) {
	sprintf(errorMessage,
		"With margin %f and %i blocks, heuristic expected %f; actually got %f",
		mf,
		blocks,
		gameExpected,
		actual);
	throw string(errorMessage);
      }      
    }
  }

  Calendar::newYearBegins();
  _labourToSow = oldLabourToSow;
  _labourToPlow = oldLabourToPlow;
  capital->setAmounts(oldCapital);
}

void Farmland::Farmer::workFields () {  
  Calendar::Season currSeason = Calendar::getCurrentSeason();
  double availableLabour = getAmount(TradeGood::Labor);
  double capFactor = capitalFactor(*this);

  switch (currSeason) {
  default:
  case Calendar::Winter:
    // Cleanup.
    // Used land becomes 'clear' for next year.
    fields[Ended] += fields[Ripe3];
    fields[Ended] += fields[Ripe2];
    fields[Ended] += fields[Ripe1];
    fields[Ended] += fields[Sowed];
    fields[Ended] += fields[Ready];
    fields[Ripe3] = fields[Ripe2] = fields[Ripe1] = fields[Sowed] = fields[Ready] = 0;
    
    // Fallow acreage grows over.
    fields[Clear] = fields[Ended];
    fields[Ended] = 0;
    break;

  case Calendar::Spring:
    // In spring we clear new fields, plow and sow existing ones.
    while (true) {
      if ((availableLabour >= _labourToSow*capFactor) && (fields[Ready] > 0)) {
	fields[Ready]--;
	fields[Sowed]++;
	availableLabour -= _labourToSow*capFactor;
      }
      else if ((availableLabour >= _labourToPlow*capFactor) && (fields[Clear] > 0)) {
	fields[Clear]--;
	fields[Ready]++;
	availableLabour -= _labourToPlow*capFactor;
      }
      /*
      // TODO: Move clearing of fields somewhere else, or divide assigned land up between farmers.
      else if ((availableLabour >= _labourToClear*capFactor) &&
      (total - fields[Clear] - fields[Ready] - fields[Sowed] - fields[Ripe1] - fields[Ripe2] - fields[Ripe3] - fields[Ended] > 0)) {
      fields[Clear]++;
      availableLabour -= _labourToClear*capFactor;
      }
      */
      else break; 
    }
    break;

  case Calendar::Summer: {
    // In summer the crops ripen and we fight the weeds.
    // If there isn't enough labour to tend the crops, they
    // degrade; if the weather is bad they don't advance.
    double weatherModifier = 1; // TODO: Insert weather-getting code here
    int untendedRipe1 = fields[Ripe1]; fields[Ripe1] = 0;
    int untendedRipe2 = fields[Ripe2]; fields[Ripe2] = 0;
    int untendedRipe3 = fields[Ripe3]; fields[Ripe3] = 0;
    while (availableLabour >= _labourToWeed * capFactor * weatherModifier) {
      availableLabour -= _labourToWeed * capFactor * weatherModifier;	
      if (fields[Sowed] > 0) {
	fields[Sowed]--;
	fields[Ripe1]++;
      }
      else if (untendedRipe1 > 0) {
	untendedRipe1--;
	fields[Ripe2]++;
      }
      else if (untendedRipe2 > 0) {
	untendedRipe2--;
	fields[Ripe3]++;
      }
      else if (untendedRipe3 > 0) {
	untendedRipe3--;
	fields[Ripe3]++;
      }
      else break; 
    }
    fields[Sowed] += untendedRipe1;
    fields[Ripe1] += untendedRipe2;
    fields[Ripe2] += untendedRipe3;
    break;
  }
  case Calendar::Autumn: {
    // In autumn we harvest.
    int block = fields[Ended] / boss->blockSize;
    int counter = fields[Ended] - block*boss->blockSize;
    double marginFactor = pow(getMarginFactor(), block);
    int harvest = min(fields[Ripe3], (int) floor(availableLabour / (_labourToReap * capFactor)));
    double totalHarvested = 0;
    availableLabour -= harvest * _labourToReap * capFactor;
    for (int i = 0; i < harvest; ++i) {
      totalHarvested += _cropsFrom3 * marginFactor;
      if (++counter >= boss->blockSize) {
	counter = 0;
	marginFactor *= getMarginFactor();
      }
    }
    fields[Ripe3] -= harvest;
    fields[Ended] += harvest;
    
    harvest = min(fields[Ripe2], (int) floor(availableLabour / (_labourToReap * capFactor)));
    availableLabour -= harvest * _labourToReap * capFactor;
    for (int i = 0; i < harvest; ++i) {
      totalHarvested += _cropsFrom2 * marginFactor;
      if (++counter >= boss->blockSize) {
	counter = 0;
	marginFactor *= getMarginFactor();
      }
    }
    fields[Ripe2] -= harvest;
    fields[Ended] += harvest;
    
    harvest = min(fields[Ripe1], (int) floor(availableLabour / (_labourToReap * capFactor)));
    availableLabour -= harvest * _labourToReap * capFactor;
    for (int i = 0; i < harvest; ++i) {
      totalHarvested += _cropsFrom1 * marginFactor;
      if (++counter >= boss->blockSize) {
	counter = 0;
	marginFactor *= getMarginFactor();
      }
    }
    fields[Ripe1] -= harvest;
    fields[Ended] += harvest;

    deliverGoods(output, totalHarvested);
    break;
  }
  } // Not a typo, ends switch.

  double usedLabour = availableLabour - getAmount(TradeGood::Labor);
  deliverGoods(TradeGood::Labor, usedLabour);
  if (getAmount(TradeGood::Labor) < 0) throw string("Negative labour after workFields");
}

void Farmland::Farmer::fillBlock (int block, vector<int>& theBlock) const {
  // Figure out the field status in the given block;
  // since block N is always worked on before block N+1,
  // we can do so by counting backwards from the top status
  // until we've reached block*blockSize fields.

  // This is a Schlemiel's Algorithm... I should provide an iterator,
  // or at least cache the previous block.
  int counted = 0;
  int found = 0;
  for (int i = Ended; i >= 0; --i) {
    if (counted + fields[i] <= block * boss->blockSize) {
      counted += fields[i];
      continue;
    }
    theBlock[i] = min(fields[i], boss->blockSize - found);
    found += theBlock[i];
    if (found >= boss->blockSize) break;
    counted += theBlock[i];
  }
}

double Farmland::Farmer::getLabourForBlock (int block) const {
  // Returns the amount of labour needed to tend the
  // fields, on the assumption that the necessary labour
  // will be spread over the remaining turns in the season. 

  Calendar::Season currSeason = Calendar::getCurrentSeason();
  if (Calendar::Winter == currSeason) return 0;

  vector<int> theBlock(NumStatus, 0);
  fillBlock(block, theBlock);
  
  double ret = 0;
  switch (currSeason) {
  default:
  case Calendar::Spring:
    // In spring we clear new fields, plow and sow existing ones.
    // Fields move from Clear to Ready to Sowed.
    ret += theBlock[Ready] * _labourToSow;
    ret += theBlock[Clear] * (_labourToPlow + _labourToSow);
    break;

  case Calendar::Summer:
    // Weeding is special: It is not done once and finished,
    // like plowing, sowing, and harvesting. It must be re-done
    // each turn. So, avoid the div-by-turns-remaining below.
    return (theBlock[Sowed] + theBlock[Ripe1] + theBlock[Ripe2] + theBlock[Ripe3]) * _labourToWeed * capitalFactor(*this);
      
  case Calendar::Autumn:
    // All reaping is equal. 
    ret += (theBlock[Ripe1] + theBlock[Ripe2] + theBlock[Ripe3]) * _labourToReap;
    break; 
  }

  ret *= capitalFactor(*this);
  ret /= Calendar::turnsToNextSeason();
  // Account for roundoff error, so we don't fail to do a task taking 100 labour
  // because it occurred in the last block and we ended up with 99.9999.
  ret += 0.00001;
  return ret; 
}

Forest::Forest ()
  : Building(1)
  , Mirrorable<Forest>()
  , yearsSinceLastTick(0)    
  , minStatusToHarvest(Huge)    
  , blockSize(1)
{
  for (int j = 0; j < numOwners; ++j) {
    foresters.push_back(new Forester(this));
  }
}

Forest::Forest (Forest* other)
  : Building(1)
  , Mirrorable<Forest>(other)
  , yearsSinceLastTick(0)    
  , minStatusToHarvest(Huge)
  , blockSize(other->blockSize)
{}

Forest::~Forest () {
  BOOST_FOREACH(Forester* forester, foresters) {
    forester->destroyIfReal();
  }
}

Forest::Forester::Forester (Forest* b)
  : Industry<Forester>(this)
  , Mirrorable<Forest::Forester>()
  , groves(NumStatus, 0)
  , tendedGroves(0)
  , boss(b)
{}

Forest::Forester::Forester (Forester* other)
  : Industry<Forester>(this)
  , Mirrorable<Forest::Forester>(other)
  , groves(NumStatus, 0)
{}

void Forest::Forester::setMirrorState () {
  mirror->owner = owner;
  for (unsigned int i = 0; i < groves.size(); ++i) mirror->groves[i] = groves[i];
  mirror->boss = boss->getMirror();
}

Forest::Forester::~Forester () {}

// NB, this is not intended to take marginal decline into
// account - "block N" is for cases, like Forester, where
// there are different outputs for other reasons than
// margins.
double Forest::Forester::outputOfBlock (int block) const {
  int counted = 0;
  for (int i = Climax; i >= boss->minStatusToHarvest; --i) {
    // This is slightly approximate.
    if (counted + groves[i] >= block * boss->blockSize) {
      return boss->blockSize * _amountOfWood[i];
    }
    counted += groves[i];
  }
  return 0;
}

void Forest::unitTests () {
  Forest testForest;
  if ((int) testForest.foresters.size() != numOwners) {
    sprintf(errorMessage, "Forest should have %i Foresters, has %i", numOwners, testForest.foresters.size());
    throw string(errorMessage);
  }

  testForest.minStatusToHarvest = Mature;
  testForest.foresters[0]->unitTests();
}

void Forest::Forester::unitTests () {
  if (!output) throw string("Forest output has not been set.");  
  // Note that this is not static, it's run on a particular Forester.
  GoodsHolder oldCapital(*capital);
  capital->clear();
  TradeGood const* testCapGood = 0;
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    if ((*tg) == output) continue;
    testCapGood = (*tg);
    capital->setAmount((*tg), 0.1);
    break;
  }

  Calendar::newYearBegins();
  groves[Clear] = 100;
  groves[Huge] = 100;
  double laborNeeded = getLabourForBlock(0);
  if (laborNeeded <= 0) {
    sprintf(errorMessage, "Expected to need positive amount of labor, but found %f", laborNeeded);
    throw string(errorMessage);
  }

  deliverGoods(testCapGood, 1);
  double laborNeededWithCapital = getLabourForBlock(0);
  if (laborNeededWithCapital >= laborNeeded) {
    sprintf(errorMessage,
	    "With capital %f of %s, should require less than %f labour, but need %f",
	    getAmount(testCapGood),
	    testCapGood->getName().c_str(),
	    laborNeeded,
	    laborNeededWithCapital);
    throw string(errorMessage);
  }
  deliverGoods(testCapGood, -1);

  // Set labour so we need 300 of it. Saving about 7% of that means we should bid
  // for capital if the capital price is below 210.
  int oldTendLabour = _labourToTend;
  int oldHarvestLabour = _labourToHarvest;
  _labourToTend = 1;
  _labourToHarvest = 1;
  laborNeeded = getLabourForBlock(0);
  GoodsHolder prices;
  vector<MarketBid*> bidlist;
  prices.deliverGoods(TradeGood::Labor, 1);
  prices.deliverGoods(testCapGood, 1);
  prices.deliverGoods(output, 1);
  getBids(prices, bidlist);
  
  double foundLabour = 0;
  bool foundCapGood = false;
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) {
      foundLabour += mb->amountToBuy;
      if (fabs(mb->amountToBuy - 300) > 0.1) {
	sprintf(errorMessage, "Expected to buy 300 %s, bid is for %f", TradeGood::Labor->getName().c_str(), mb->amountToBuy);
	throw string(errorMessage);
      }
    }
    else if (mb->tradeGood == testCapGood) {
      foundCapGood = true;
      if (mb->amountToBuy <= 0) {
	sprintf(errorMessage, "Expected to buy %s, but am selling", testCapGood->getName().c_str());
	throw string(errorMessage);
      }
    }
    else {
      sprintf(errorMessage,
	      "Expected to bid on %s and %s, but got bid for %f %s (capital %f price %f) %i %i %f",
	      TradeGood::Labor->getName().c_str(),
	      testCapGood->getName().c_str(),
	      mb->amountToBuy,
	      mb->tradeGood->getName().c_str(),
	      capital->getAmount(mb->tradeGood),
	      prices.getAmount(mb->tradeGood),
	      mb->tradeGood->getIdx(),
	      testCapGood->getIdx(),
	      capital->getAmount(testCapGood));
      throw string(errorMessage);
    }
  }
  if (0.1 > foundLabour) {
    sprintf(errorMessage,
	    "Expected to buy %s, no bid found (%f %f)",
	    TradeGood::Labor->getName().c_str(),
	    outputOfBlock(0),
	    getLabourForBlock(0));
    throw string(errorMessage);
  }
  if (!foundCapGood) {
    sprintf(errorMessage, "Expected to buy %s, no bid found", testCapGood->getName().c_str());
    throw string(errorMessage);
  }

  owner = this;
  tendedGroves = getTendedArea();
  tendedGroves -= 10;
  workGroves(true);
  if ((groves[Clear] > 0) || (groves[Huge] > 0) || (groves[Climax] != 95) || (groves[Planted] != 95) || (groves[Wild] != 10)) {
    sprintf(errorMessage,
	    "Expected (Clear, Huge, Climax, Planted, Wild) to be (%i %i %i %i %i), got (%i %i %i %i %i)",
	    0, 0, 95, 95, 10,
	    groves[Clear],
	    groves[Huge],
	    groves[Climax],
	    groves[Planted],
	    groves[Wild]);
    throw string(errorMessage);
  }

  deliverGoods(TradeGood::Labor, getLabourForBlock(0));
  deliverGoods(TradeGood::Labor, labourForMaintenance());
  workGroves(false);
  double firstOutput = getAmount(output);
  if (0.01 > firstOutput) throw string("Expected to have some wood after workGroves");

  if (0 >= boss->blockSize) throw new string("Expected positive block size");
  for (unsigned int i = 0; i < groves.size(); ++i) groves[i] = 0;
  groves[Climax] = boss->blockSize;
  setAmount(TradeGood::Labor, getLabourForBlock(0));
  setAmount(output, 0);
  tendedGroves = getTendedArea();
  workGroves(false);
  firstOutput = getAmount(output);

  vector<double> marginFactors;
  marginFactors.push_back(0.99);
  marginFactors.push_back(0.9);
  marginFactors.push_back(0.1);
  BOOST_FOREACH(double mf, marginFactors) {
    boss->marginFactor = mf;
    for (int blocks = 2; blocks < 5; ++blocks) {
      groves[Climax] = blocks*boss->blockSize;
      double gameExpected = 0;      
      for (int j = 0; j < blocks; ++j) gameExpected += outputOfBlock(j) * pow(mf, j);
      setAmount(output, 0);
      setAmount(TradeGood::Labor, blocks*getLabourForBlock(0));
      tendedGroves = getTendedArea();
      workGroves(false);
      // Geometric sum, with growth rate r, from 0 to n, is (1-r^(n+1)) / (1-r).
      double coderExpected = firstOutput * (1 - pow(mf, blocks)) / (1 - mf);
      double actual = getAmount(output);
      if (0.1 < fabs(coderExpected - actual)) {
	sprintf(errorMessage,
		"With margin %f and %i blocks, expected %f but got %f",
		mf,
		blocks,
		coderExpected,
		actual);
	throw string(errorMessage);
      }
      if (0.1 < fabs(gameExpected - actual)) {
	sprintf(errorMessage,
		"With margin %f and %i blocks, heuristic expected %f; actually got %f",
		mf,
		blocks,
		gameExpected,
		actual);
	throw string(errorMessage);
      }      
    }
  }
  
  _labourToTend = oldTendLabour;
  _labourToHarvest = oldHarvestLabour;
  capital->setAmounts(oldCapital);
}

double Forest::Forester::getLabourForBlock (int block) const {
  Calendar::Season currSeason = Calendar::getCurrentSeason();
  if (Calendar::Winter == currSeason) return 0;
  return _labourToHarvest * boss->blockSize * capitalFactor(*this);
}

double Forest::Forester::labourForMaintenance () const {
  Calendar::Season currSeason = Calendar::getCurrentSeason();
  if (Calendar::Winter == currSeason) return 0;
  return _labourToTend * getTendedArea() * capitalFactor(*this);
}

double Forest::Forester::lossFromNoMaintenance () const {
  double ret = 0;
  
  int wildening = getTendedArea() - tendedGroves;
  int target = Climax;
  static vector<int> workingSpace;
  workingSpace = groves;
  while (wildening > 0) {
    if (0 < workingSpace[target]) {
      ret += _amountOfWood[target];
      --wildening;
      --workingSpace[target];
    }
    if (--target < Clear) target = Climax;
  }

  return ret;
}

int Forest::Forester::numBlocks () const {
  int ret = 0;
  for (int i = Climax; i >= boss->minStatusToHarvest; --i) {
    ret += groves[i];
  }

  ret /= boss->blockSize;
  return ret; 

}

void Forest::Forester::workGroves (bool tick) {
  if (tick) {
    groves[Climax]    += groves[Huge];
    groves[Huge]       = groves[Mighty];
    groves[Mighty]     = groves[Mature];
    groves[Mature]     = groves[Grown];
    groves[Grown]      = groves[Young];
    groves[Young]      = groves[Saplings];
    groves[Saplings]   = groves[Scrub];
    groves[Scrub]      = groves[Planted];
    groves[Planted]    = groves[Clear];
    groves[Clear]      = 0;
    int wildening = getTendedArea() - tendedGroves;
    groves[Wild] += wildening;
    int target = Climax;
    while (wildening > 0) {
      if (0 < groves[target]) {
	groves[target]--;
	wildening--;
      }
      if (--target < Clear) target = Climax;
    }
    tendedGroves = 0;
    return;
  }

  double availableLabour = getAmount(TradeGood::Labor);
  double capFactor = capitalFactor(*this);
  
  // First priority: Ensure tended land doesn't go wild.
  int grovesToTend = getTendedArea() - tendedGroves;
  if (grovesToTend > 0) {
    int tended = min((int) floor(availableLabour / (_labourToTend * capFactor)), grovesToTend);
    tendedGroves += tended;
    availableLabour -= tended * _labourToTend * capFactor;
  }
  double decline = 1;
  double totalChopped = 0;
  int blockCounter = 0;
  for (int i = Climax; i >= boss->minStatusToHarvest; --i) {
    if (availableLabour < _labourToHarvest*capFactor) break;
    while (0 < groves[i]) {
      availableLabour -= _labourToHarvest * capFactor;  
      --groves[i];
      totalChopped += _amountOfWood[i] * decline;
      
      if (++blockCounter >= boss->blockSize) {
	blockCounter = 0;
	decline *= getMarginFactor();
      }
      if (availableLabour < _labourToHarvest*capFactor) break;
    }
  }
  deliverGoods(output, totalChopped);
  
  double usedLabour = availableLabour - getAmount(TradeGood::Labor);
  deliverGoods(TradeGood::Labor, usedLabour);
  if (getAmount(TradeGood::Labor) < 0) throw string("Negative labour after workGroves");
}

int Forest::Forester::getTendedArea () const {
  return (groves[Clear] +
	  groves[Planted] +
	  groves[Scrub] +
	  groves[Saplings] +
	  groves[Young] +
	  groves[Grown] +
	  groves[Mature] +
	  groves[Mighty] +
	  groves[Huge] +
	  groves[Climax]);
}

int Forest::Forester::getForestArea () const {
  return getTendedArea() + groves[Wild];
}

void Forest::setMirrorState () {
  mirror->setOwner(getOwner());
  mirror->foresters.clear();
  BOOST_FOREACH(Forester* forester, foresters) {
    forester->setMirrorState();
    mirror->foresters.push_back(forester->getMirror());
  }
  mirror->yearsSinceLastTick = yearsSinceLastTick;
  mirror->minStatusToHarvest = minStatusToHarvest;
}

void Village::setMirrorState () {
  women.setMirrorState();
  males.setMirrorState();
  milTrad->setMirrorState();
  mirror->milTrad = milTrad->getMirror();
  mirror->consumptionLevel = consumptionLevel;
  mirror->setOwner(getOwner());
  mirror->setAmounts(this);
  // Building mirror states set by Hex.  
  if (farm) mirror->farm = farm->getMirror();
}

void Farmland::setMirrorState () {
  mirror->setOwner(getOwner());
  mirror->farmers.clear();
  BOOST_FOREACH(Farmer* farmer, farmers) {
    farmer->setMirrorState();
    mirror->farmers.push_back(farmer->getMirror());
  }
  mirror->blockSize = blockSize;
}

double Village::production () const {
  double ret = 0;
  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    ret += (males.getPop(i) + femaleProduction*women.getPop(i)) * products[i]; 
  }

  ret -= milTrad->getRequiredWork();
  ret -= workedThisTurn;
  return ret;
}

double Village::consumption () const {
  double ret = 0;
  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    ret += consume[i]*(males.getPop(i) + femaleConsumption*women.getPop(i));
  }
  return ret; 
}

void Farmland::devastate (int devastation) {
  while (devastation > 0) {
    int target = rand() % farmers.size();
    if      (farmers[target]->fields[Ripe3] > 0) {farmers[target]->fields[Ripe3]--; farmers[target]->fields[Ripe2]++;}
    else if (farmers[target]->fields[Ripe2] > 0) {farmers[target]->fields[Ripe2]--; farmers[target]->fields[Ripe1]++;}
    else if (farmers[target]->fields[Ripe1] > 0) {farmers[target]->fields[Ripe1]--; farmers[target]->fields[Ready]++;}
    devastation--;
  }
}

void Farmland::endOfTurn () {
  BOOST_FOREACH(Farmer* farmer, farmers) farmer->workFields();
  countTotals();
}

void Farmland::delivery (EconActor* target, TradeGood const* const good, double amount) {
  unsigned int divs = 0;
  BOOST_FOREACH(Farmer* farmer, farmers) {
    if (farmer->isOwnedBy(target)) ++divs;
  }

  if (0 == divs) return;
  amount /= divs;
  BOOST_FOREACH(Farmer* farmer, farmers) {  
    if (!farmer->isOwnedBy(target)) continue;
    farmer->deliverGoods(good, amount);
  }
}

void Village::eatFood () {
  double consumptionFactor = consumption();
  consumptionLevel = 0;
  BOOST_FOREACH(MaslowLevel const* level, maslowLevels) {
    bool canGetLevel = true;
    for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
      double amountNeeded = level->getAmount(*tg) * consumptionFactor;
      if (amountNeeded <= getAmount(*tg)) continue;
      canGetLevel = false;
      break;
    }
    if (!canGetLevel) break;
    for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
      double amountNeeded = level->getAmount(*tg) * consumptionFactor;
      deliverGoods((*tg), -amountNeeded);
    }
    consumptionLevel = level;
  }
}

void Farmland::setDefaultOwner (EconActor* o) {
  if (!o) return;
  BOOST_FOREACH(Farmer* farmer, farmers) {  
    if (farmer->getEconOwner()) continue;
    farmer->setEconOwner(o);
  }
}

void Forest::setDefaultOwner (EconActor* o) {
  if (!o) return;
  BOOST_FOREACH(Forester* forester, foresters) {
    if (forester->getEconOwner()) continue;
    forester->setEconOwner(o);
  }
}

void Forest::endOfTurn () {
  bool tick = false;
  if (Calendar::Winter == Calendar::getCurrentSeason()) {
    ++yearsSinceLastTick;
    if (yearsSinceLastTick >= 5) {
      yearsSinceLastTick = 0;
      tick = true;
    }
  }
  BOOST_FOREACH(Forester* forester, foresters) forester->workGroves(tick);
}

void Farmland::countTotals () {
  totalFields[Clear] = 0;
  totalFields[Ready] = 0;
  totalFields[Sowed] = 0;
  totalFields[Ripe1] = 0;
  totalFields[Ripe2] = 0;
  totalFields[Ripe3] = 0;
  totalFields[Ended] = 0;

  BOOST_FOREACH(Farmer* farmer, farmers) {
    totalFields[Clear] += farmer->fields[Clear];
    totalFields[Ready] += farmer->fields[Ready];
    totalFields[Sowed] += farmer->fields[Sowed];
    totalFields[Ripe1] += farmer->fields[Ripe1];
    totalFields[Ripe2] += farmer->fields[Ripe2];
    totalFields[Ripe3] += farmer->fields[Ripe3];
    totalFields[Ended] += farmer->fields[Ended];
  }
}

int Village::produceRecruits (MilUnitTemplate const* const recruitType, MilUnit* target, Outcome dieroll) {
  double luckModifier = 1.0;
  switch (dieroll) {
  case VictoGlory: luckModifier = 1.50; break;
  case Good:       luckModifier = 1.25; break;
  case Bad:        luckModifier = 0.90; break;
  case Disaster:   luckModifier = 0.75; break;
  case Neutral:
  default:
    break; 
  }

  static AgeTracker recruits;
  recruits.clear(); 
  int recruited = 0;
  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    int numMen = males.getPop(i);
    if (1 > numMen) continue; 
    double femaleSurplus = women.getPop(i);
    femaleSurplus /= numMen; 
    femaleSurplus -= femaleSurplusZero; 

    int curr = convertFractionToInt(numMen * recruitChance[i] * luckModifier * exp(femaleSurplusEffect*femaleSurplus));
    if (curr > numMen) curr = numMen;
    if (recruited + curr > recruitType->recruit_speed) curr = recruitType->recruit_speed - recruited;
    if (1 > curr) continue;
    recruits.addPop(curr, i);
    males.addPop(-curr, i);
    recruited += curr; 
    if (recruited >= recruitType->recruit_speed) break; 
  }

  target->addElement(recruitType, recruits); 

  return recruited; 
}

Mine::Mine ()
  : Building(1)
  , Mirrorable<Mine>()
  , veinsPerMiner(1)
{
  for (int j = 0; j < numOwners; ++j) {
    miners.push_back(new Miner(this));
  }
}

Mine::Mine (Mine* other)
  : Building(1)
  , Mirrorable<Mine>(other)
{}

Mine::~Mine () {
  BOOST_FOREACH(Miner* miner, miners) {
    miner->destroyIfReal();
  }
}

Mine::Miner::Miner (Mine* m)
  : Industry<Miner>(this)
  , Mirrorable<Miner>()
  , shafts(MineStatus::numTypes(), 0)
  , mine(m)
{}

Mine::Miner::Miner (Miner* other)
  : Industry<Miner>(this)
  , Mirrorable<Mine::Miner>(other)
  , shafts(MineStatus::numTypes(), 0)
{}

void Mine::setDefaultOwner (EconActor* o) {
  if (!o) return;
  BOOST_FOREACH(Miner* miner, miners) {  
    if (miner->getEconOwner()) continue;
    miner->setEconOwner(o);
  }
}

void Mine::Miner::setMirrorState () {
  mirror->owner = owner;
  for (unsigned int i = 0; i < shafts.size(); ++i) mirror->shafts[i] = shafts[i];
  mirror->mine = mine->getMirror();
}

Mine::Miner::~Miner () {}

Mine::MineStatus::MineStatus (string n, int rl, bool lastOne)
  : Enumerable<MineStatus>(this, n, lastOne)
  , requiredLabour(rl)
{}

Mine::MineStatus::~MineStatus () {}

void Mine::endOfTurn () {
  BOOST_FOREACH(Miner* miner, miners) miner->workShafts();
}

double Mine::Miner::outputOfBlock (int block) const {
  return _amountOfIron;
}

void Mine::unitTests () {
  if (3 > MineStatus::numTypes()) {
    sprintf(errorMessage, "Expected at least 3 MineStatus entries, got %i", MineStatus::numTypes());
    throw string(errorMessage);
  }
  
  Mine testMine;
  if ((int) testMine.miners.size() != numOwners) {
    sprintf(errorMessage, "Mine should have %i Miners, has %i", numOwners, testMine.miners.size());
    throw string(errorMessage);
  }
  testMine.miners[0]->unitTests();
}

void Mine::Miner::unitTests () {
  if (!output) throw string("Mine output has not been set.");  
  // Note that this is not static, it's run on a particular Miner.
  GoodsHolder oldCapital(*capital);
  TradeGood const* testCapGood = 0;
  capital->clear();
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    testCapGood = (*tg);
    capital->setAmount((*tg), 0.1);
    break;
  }

  MineStatus::Iter msi = MineStatus::start();
  MineStatus* status = (*msi);
  shafts[*status] = 100;
  double laborNeeded = getLabourForBlock(0);
  if (laborNeeded <= 0) {
    sprintf(errorMessage, "Expected to need positive amount of labor, but found %f", laborNeeded);
    throw string(errorMessage);
  }

  deliverGoods(testCapGood, 1);
  double laborNeededWithCapital = getLabourForBlock(0);
  if (laborNeededWithCapital >= laborNeeded) {
    sprintf(errorMessage,
	    "With capital %f of %s, should require less than %f labour, but need %f",
	    getAmount(testCapGood),
	    testCapGood->getName().c_str(),
	    laborNeeded,
	    laborNeededWithCapital);
    throw string(errorMessage);
  }
  deliverGoods(testCapGood, -1);

  laborNeeded = getLabourForBlock(0);
  GoodsHolder prices;
  vector<MarketBid*> bidlist;
  prices.deliverGoods(TradeGood::Labor, 1);
  prices.deliverGoods(testCapGood, 1);
  prices.deliverGoods(output, 1);
  getBids(prices, bidlist);
  
  double foundLabour = 0;
  bool foundCapGood = false;
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) {
      foundLabour += mb->amountToBuy;
      if (fabs(mb->amountToBuy - status->requiredLabour) > 0.1) {
	sprintf(errorMessage, "Expected to buy %i %s, bid is for %f", status->requiredLabour, TradeGood::Labor->getName().c_str(), mb->amountToBuy);
	throw string(errorMessage);
      }
    }
    else if (mb->tradeGood == testCapGood) {
      foundCapGood = true;
      if (mb->amountToBuy <= 0) {
	sprintf(errorMessage, "Expected to buy %s, but am selling", testCapGood->getName().c_str());
	throw string(errorMessage);
      }
    }
    else {
      sprintf(errorMessage,
	      "Expected to bid on %s and %s, but got bid for %f %s (capital %f price %f) %i %i %f",
	      TradeGood::Labor->getName().c_str(),
	      testCapGood->getName().c_str(),
	      mb->amountToBuy,
	      mb->tradeGood->getName().c_str(),
	      capital->getAmount(mb->tradeGood),
	      prices.getAmount(mb->tradeGood),
	      mb->tradeGood->getIdx(),
	      testCapGood->getIdx(),
	      capital->getAmount(testCapGood));
      throw string(errorMessage);
    }
  }

  if (0.1 > foundLabour) throwFormatted("Expected to buy %s, no bid found (%f %f)",
					TradeGood::Labor->getName().c_str(),
					outputOfBlock(0),
					getLabourForBlock(0));
  if (!foundCapGood) throwFormatted("Expected to buy %s, no bid found", testCapGood->getName().c_str());

  owner = this;
  deliverGoods(TradeGood::Labor, laborNeeded);
  workShafts();
  if (shafts[*status] != 99) throwFormatted("Expected %s to be 99, got %i", status->getName().c_str(), shafts[*status]);

  double productionOne = getAmount(output);
  if (0.01 > productionOne) throw string("Expected to have some iron after workShafts");

  // Check that, with marginal production of zero, we get the same labor bid.
  mine->veinsPerMiner = 2;
  mine->marginFactor = 0;
  bidlist.clear();
  getBids(prices, bidlist);
  double newFoundLabour = 0;
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) newFoundLabour += mb->amountToBuy;
  }
  if (fabs(foundLabour - newFoundLabour) > 0.1) {
    sprintf(errorMessage, "With zero margin, expected to buy %f, but got %f", foundLabour, newFoundLabour);
    throw string(errorMessage);
  }

  // With no marginal decline, should buy twice as much.
  mine->marginFactor = 1;
  bidlist.clear();
  getBids(prices, bidlist);
  newFoundLabour = 0;
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) newFoundLabour += mb->amountToBuy;
  }
  if (fabs(mine->veinsPerMiner*foundLabour - newFoundLabour) > 0.1) {
    sprintf(errorMessage, "With margin, 1 expected to buy %f, but got %f", mine->veinsPerMiner*foundLabour, newFoundLabour);
    throw string(errorMessage);
  }

  setAmount(output, 0);
  setAmount(TradeGood::Labor, numBlocks() * getLabourForBlock(0));
  workShafts();
  double productionTwo = getAmount(output);
  if (0.1 < fabs(productionTwo - numBlocks() * productionOne)) {
    sprintf(errorMessage, "With %i blocks and margin factor 1, expected to produce %f, but got %f", numBlocks(), numBlocks() * productionOne, productionTwo);
    throw string(errorMessage);
  }

  mine->marginFactor = 0.5;
  setAmount(output, 0);
  setAmount(TradeGood::Labor, numBlocks() * getLabourForBlock(0));
  workShafts();
  double productionThree = getAmount(output);
  if (0.1 < fabs(productionThree - 1.5 * productionOne)) {
    sprintf(errorMessage, "With two blocks and margin factor 0.5, expected to produce %f, but got %f", 1.5 * productionOne, productionThree);
    throw string(errorMessage);
  }

  mine->marginFactor = 0.75;
  mine->veinsPerMiner = 3;
  setAmount(output, 0);
  setAmount(TradeGood::Labor, numBlocks() * getLabourForBlock(0));
  workShafts();
  double productionFour = getAmount(output);
  if (0.1 < fabs(productionFour - 2.3125 * productionOne)) {
    sprintf(errorMessage, "With three blocks and margin factor 0.75, expected to produce %f, but got %f", 2.3125 * productionOne, productionThree);
    throw string(errorMessage);
  }
  
  capital->setAmounts(oldCapital);
}

double Mine::Miner::getLabourForBlock (int block) const {
  double ret = 0;
  for (MineStatus::Iter ms = MineStatus::start(); ms != MineStatus::final(); ++ms) {
    if (0 == shafts[**ms]) continue;
    ret = (*ms)->requiredLabour;
    break;
  }

  ret *= capitalFactor(*this);
  return ret; 
}

void Mine::Miner::workShafts () {
  double availableLabour = getAmount(TradeGood::Labor);
  double capFactor = capitalFactor(*this);

  double decline = 1;
  for (MineStatus::Iter ms = MineStatus::start(); ms != MineStatus::final(); ++ms) {
    if (0 == shafts[**ms]) continue;
    for (int i = 0; i < numBlocks(); ++i) {
      if (availableLabour < (*ms)->requiredLabour * capFactor) break;
      availableLabour -= (*ms)->requiredLabour * capFactor;
      deliverGoods(output, _amountOfIron * decline);
      if (0 == --shafts[**ms]) break;
      decline *= getMarginFactor();
    }
    break;
  }

  double usedLabour = availableLabour - getAmount(TradeGood::Labor);
  deliverGoods(TradeGood::Labor, usedLabour);
  if (getAmount(TradeGood::Labor) < 0) throw string("Negative labour after workShafts");
}

void Mine::setMirrorState () {
  mirror->setOwner(getOwner());
  mirror->miners.clear();
  BOOST_FOREACH(Miner* miner, miners) {
    miner->setMirrorState();
    mirror->miners.push_back(miner->getMirror());
  }
}
