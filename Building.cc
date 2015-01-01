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
vector<GoodsHolder> Village::maslowLevels;

int Farmland::_labourToSow    = 1;
int Farmland::_labourToPlow   = 10;
int Farmland::_labourToClear  = 100;
int Farmland::_labourToWeed   = 3;
int Farmland::_labourToReap   = 10;
int Farmland::_cropsFrom3     = 1000;
int Farmland::_cropsFrom2     = 500;
int Farmland::_cropsFrom1     = 100;
TradeGood const* Farmland::output = 0;

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
  //if (isReal()) Logger::logStream(Logger::Debug) << "Recruited " << newSoldiers << " " << recruitType->name << "\n"; 
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
  , foodMortalityModifier(1)
  , farm(0)
  , workedThisTurn(0)
{
  milTrad = new MilitiaTradition();
}

Village::Village (Village* other)
  : Mirrorable<Village>(other)
  , milTrad(0)
  , foodMortalityModifier(1)
  , farm(0)    
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

  double moneyAvailable    = getAmount(TradeGood::Money);
  double consumptionFactor = consumption();
  double laborAvailable    = production();
  GoodsHolder amountToBuy;
  for (vector<GoodsHolder>::iterator level = maslowLevels.begin(); level != maslowLevels.end(); ++level) {
    double moneyNeeded = 0;
    bool canGetLevel = true;
    for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
      double amountNeeded = (*level).getAmount(*tg) * consumptionFactor;
      if (amountNeeded < 0.001) continue;
      if (TradeGood::Labor == (*tg)) {
	// Deal with labour specially since we produce it and presumably don't need to buy.
	if (amountNeeded > laborAvailable) {
	  canGetLevel = false;
	  break;
	}
	else {
	  laborAvailable -= amountNeeded;
	  continue;
	}
      }
      moneyNeeded += prices.getAmount(*tg) * amountNeeded;
      if (moneyNeeded > moneyAvailable + laborAvailable*prices.getAmount(TradeGood::Labor)) {
	canGetLevel = false;
	break;
      }
    }
    if (!canGetLevel) break;

    for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
      double amountNeeded = (*level).getAmount(*tg) * consumptionFactor;
      if (amountNeeded < 0.001) continue;
      moneyNeeded = prices.getAmount(*tg) * amountNeeded;
      double laborNeeded = moneyNeeded / prices.getAmount(TradeGood::Labor);
      amountToBuy.deliverGoods((*tg), amountNeeded);
      amountToBuy.deliverGoods(TradeGood::Labor, -laborNeeded);
    }
  }

  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    double amount = amountToBuy.getAmount(*tg);
    if (fabs(amount) < 1) continue;
    bidlist.push_back(new MarketBid((*tg), amount, this));
  }
}

void Village::unitTests () {
  Village testVillage;
  testVillage.males.addPop(1000, 20); // 1000 20-year-old males... wonder what they do for entertainment?
  GoodsHolder prices;
  vector<MarketBid*> bidlist;
  prices.deliverGoods(TradeGood::Labor, 1);
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    prices.deliverGoods((*tg), 0.25);
  }
  testVillage.getBids(prices, bidlist);
  if (0 == bidlist.size()) throw string("Village should have made at least one bid.");
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if ((mb->tradeGood == TradeGood::Labor) && (mb->amountToBuy > 0)) throw string("Should be selling, not buying, labor.");
    else if ((mb->tradeGood != TradeGood::Labor) && (mb->amountToBuy < 0)) throw string("Should be buying, not selling, ") + mb->tradeGood->getName();
  }

  double labor = testVillage.produceForContract(TradeGood::Labor, 100);
  if (fabs(labor - 100) > 0.1) {
    char number[500];
    sprintf(number, "Village should have produced 100 labor, got %f", labor);    
    throw string(number);
  }
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
  if (male) return baseMaleMortality[age]*foodMortalityModifier;
  return baseFemaleMortality[age]*foodMortalityModifier;
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

  Calendar::Season currSeason = Calendar::getCurrentSeason();
  if (Calendar::Winter != currSeason) return;

  males.age();
  women.age();
  
  for (int i = 1; i < AgeTracker::maxAge; ++i) {
    double fracLoss = adjustedMortality(i, true) * males.getPop(i);
    males.addPop(-convertFractionToInt(fracLoss), i);
    
    fracLoss = adjustedMortality(i, false) * women.getPop(i);
    women.addPop(-convertFractionToInt(fracLoss), i);
  }

  milTrad->decayTradition(); 

  MilUnitGraphicsInfo* milGraph = (MilUnitGraphicsInfo*) milTrad->militia->getGraphicsInfo();  
  if (milGraph) milGraph->updateSprites(milTrad); 
  
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
      double pregnancies = pairs * fertility[fAge];
      takenWomen[fAge] += (int) floor(pairs);
      popIncrease += convertFractionToInt(pregnancies); 
    }
  }

  males.addPop((int) floor(0.5 * popIncrease + 0.5), 0);
  women.addPop((int) floor(0.5 * popIncrease + 0.5), 0);

  updateMaxPop(); 
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
  : Building()
  , Mirrorable<Farmland>()
{
  for (int j = 0; j < numOwners; ++j) {
    farmers.push_back(new Farmer());
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
  : Mirrorable<Farmland>(other)
{}

Farmland::Farmer::Farmer ()
  : Mirrorable<Farmland::Farmer>()
  , fields(NumStatus, 0)
{}

Farmland::Farmer::Farmer (Farmer* other)
  : Mirrorable<Farmland::Farmer>(other)
  , fields(NumStatus, 0)
{}

void Farmland::Farmer::setMirrorState () {
  mirror->owner = owner;
  for (unsigned int i = 0; i < fields.size(); ++i) mirror->fields[i] = fields[i];
}

Farmland::Farmer::~Farmer () {}

void Farmland::Farmer::getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {
  // Goal is to maximise profit. Calculate how much labor we need to get in the harvest;
  // if the price of the expected food is more, bid for enough labor to do this turn's work.
  // If there is money left over, bid for equipment to reduce the amount of labor needed
  // next turn, provided the net-present-value of the reduction is higher than the cost
  // of the machinery.

  double laborNeeded = getNeededLabour();
  double expectedProduction = expectedOutput();
  if (prices.getAmount(TradeGood::Labor) * laborNeeded < prices.getAmount(output) * expectedProduction) {
    bidlist.push_back(new MarketBid(TradeGood::Labor, laborNeeded, this));
  }
  else {
    // These fields cannot be economically worked.
    // TODO: Query village about whether to do it
    // anyway as subsistence agriculture. 
  }
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    if (capital[**tg] < 0.00001) continue;
    double laborSaving = laborNeeded * (1 - marginalCapFactor((*tg), getAmount(*tg)));
    // Assuming discount rate of 10%. Present value of amount x every period to infinity is (x/r) with r the interest rate.
    // TODO: Take decay into account. Variable discount rate?
    double npv = laborSaving * prices.getAmount(TradeGood::Labor) * 10;
    if (npv > prices.getAmount(*tg)) bidlist.push_back(new MarketBid((*tg), 1, this));
  }
}

double Farmland::Farmer::expectedOutput () const {
  return _cropsFrom3 * (fields[Clear] + fields[Ready] + fields[Sowed] + fields[Ripe1] + fields[Ripe2] + fields[Ripe3]);
}

double Farmland::Farmer::produceForContract (TradeGood const* const tg, double amount) {
  return owner->produceForContract(tg, amount);
}

void Farmland::unitTests () {
  if (!output) throw string("Farm output has not been set.");
  Farmland testFarm;
  // Stupidest imaginable test, but did actually catch a problem, so...
  if ((int) testFarm.farmers.size() != numOwners) {
    char errorMessage[500];
    sprintf(errorMessage, "Farm should have %i Farmers, has %i", numOwners, testFarm.farmers.size());
    throw string(errorMessage);
  }

  testFarm.farmers[0]->unitTests();
}

void Farmland::Farmer::unitTests () {
  // Note that this is not static, it's run on a particular Farmer.
  char errorMessage[500];  
  double* oldCapital = capital;
  capital = new double[TradeGood::numTypes()];
  TradeGood const* testCapGood = 0;
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    capital[**tg] = 0;
    if ((*tg) == output) continue;
    if (testCapGood) continue;
    testCapGood = (*tg);
    capital[**tg] = 0.1;
  }

  Calendar::newYearBegins();
  fields[Clear] = 1400;
  double laborNeeded = getNeededLabour();
  if (laborNeeded <= 0) {
    sprintf(errorMessage, "Expected to need positive amount of labor, but found %f", laborNeeded);
    throw string(errorMessage);
  }

  deliverGoods(testCapGood, 1);
  double laborNeededWithCapital = getNeededLabour();
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

  // This makes us need 100 labour per Spring turn on 1400 clear fields, if capital is zero.
  // With capital 1, we need 100 * (1 - 0.1 log(2)) = 93. So we are saving
  // 7 labor with 1 iron. At price 1, that's net-present-value of 70. So,
  // there should be a bid for iron if the price is below seventy, but not if
  // it is above.
  int oldLabourToSow = _labourToSow;
  int oldLabourToPlow = _labourToPlow;
  _labourToSow = 0;
  _labourToPlow = 1;
  laborNeeded = getNeededLabour();
  GoodsHolder prices;
  vector<MarketBid*> bidlist;
  prices.deliverGoods(TradeGood::Labor, 1);
  prices.deliverGoods(testCapGood, 1);
  prices.deliverGoods(output, 1);
  getBids(prices, bidlist);
  
  if (2 != bidlist.size()) {
    sprintf(errorMessage, "Expected to make 2 bids, got %i", bidlist.size());
    throw string(errorMessage);
  }
  bool foundLabour = false;
  bool foundCapGood = false;
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) {
      foundLabour = true;
      if (fabs(mb->amountToBuy - 100) > 0.1) {
	sprintf(errorMessage, "Expected to buy 100 %s, bid is for %f", TradeGood::Labor->getName().c_str(), mb->amountToBuy);
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
	      "Expected to bid on %s and %s, but got bid for %f %s (capital %f price %f) %i %i %f %p %p",
	      TradeGood::Labor->getName().c_str(),
	      testCapGood->getName().c_str(),
	      mb->amountToBuy,
	      mb->tradeGood->getName().c_str(),
	      capital[*(mb->tradeGood)],
	      prices.getAmount(mb->tradeGood),
	      mb->tradeGood->getIdx(),
	      testCapGood->getIdx(),
	      capital[*testCapGood],
	      capital,
	      oldCapital);
      throw string(errorMessage);
    }
  }
  if (!foundLabour) {
    sprintf(errorMessage, "Expected to buy %s, no bid found", TradeGood::Labor->getName().c_str());
    throw string(errorMessage);
  }
  if (!foundCapGood) {
    sprintf(errorMessage, "Expected to buy %s, no bid found", testCapGood->getName().c_str());
    throw string(errorMessage);
  }

  owner = this;
  while (Calendar::getCurrentSeason() != Calendar::Winter) {
    deliverGoods(TradeGood::Labor, getNeededLabour());
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
  if (fabs(getAmount(output) - _cropsFrom3*1400) > 100) {
    sprintf(errorMessage, "Expected to have %f %s, but have %f", _cropsFrom3*1400.0, output->getName().c_str(), getAmount(output));
    throw string(errorMessage);
  }
  Calendar::newYearBegins();
  
  delete[] capital;
  _labourToSow = oldLabourToSow;
  _labourToPlow = oldLabourToPlow;
  capital = oldCapital;
}

void Village::setMirrorState () {
  women.setMirrorState();
  males.setMirrorState();
  milTrad->setMirrorState();
  mirror->milTrad = milTrad->getMirror();
  mirror->foodMortalityModifier = foodMortalityModifier; 
  mirror->setOwner(getOwner());
  mirror->setAmounts(this);
  if (farm) mirror->farm = farm->getMirror();
  // Farm mirror state set by Hex.
}

void Farmland::setMirrorState () {
  mirror->setOwner(getOwner());
  mirror->farmers.clear();
  BOOST_FOREACH(Farmer* farmer, farmers) {
    farmer->setMirrorState();
    mirror->farmers.push_back(farmer->getMirror());
  }
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
    int target = rand() % numOwners;
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
  for (int i = 0; i < numOwners; ++i) {
    if (farmers[i]->isOwnedBy(target)) ++divs;
  }

  if (0 == divs) return;
  amount /= divs; 
  for (int i = 0; i < numOwners; ++i) {
    if (!farmers[i]->isOwnedBy(target)) continue;
    farmers[i]->deliverGoods(good, amount);
  }
}

double Farmland::Farmer::getNeededLabour () const {
  // Returns the amount of labour needed to tend the
  // fields, on the assumption that the necessary labour
  // will be spread over the remaining turns in the season. 

  Calendar::Season currSeason = Calendar::getCurrentSeason();
  if (Calendar::Winter == currSeason) return 0;
  
  double ret = 0;
  switch (currSeason) {
  default:
  case Calendar::Spring:
    // In spring we clear new fields, plow and sow existing ones.
    // Fields move from Clear to Ready to Sowed.
    ret += fields[Ready] * _labourToSow;
    ret += fields[Clear] * (_labourToPlow + _labourToSow);
    break;

  case Calendar::Summer:
    // Weeding is special: It is not done once and finished,
    // like plowing, sowing, and harvesting. It must be re-done
    // each turn. So, avoid the div-by-turns-remaining below.
    return (fields[Sowed] + fields[Ripe1] + fields[Ripe2] + fields[Ripe3]) * _labourToWeed * capitalFactor(*this);
      
  case Calendar::Autumn:
    // All reaping is equal. 
    ret += (fields[Ripe1] + fields[Ripe2] + fields[Ripe3]) * _labourToReap;
    break; 
  }

  ret *= capitalFactor(*this);
  ret /= Calendar::turnsToNextSeason();
  return ret; 
}

void Village::eatFood () {
  double needed = consumption();
  double available = supplies;
  available /= Calendar::turnsToAutumn();
  double fraction = available/needed;
  if (fraction > 2) fraction = 2;
  supplies -= fraction * needed;
  foodMortalityModifier = 1.0 / std::max(0.1, fraction); 
}

void Farmland::setDefaultOwner (EconActor* o) {
  if (!o) return;
  for (int i = 0; i < numOwners; ++i) {
    if (farmers[i]->getEconOwner()) continue;
    farmers[i]->setEconOwner(o);
  }
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
    int harvest = min(fields[Ripe3], (int) floor(availableLabour / (_labourToReap * capFactor)));
    availableLabour -= harvest * _labourToReap * capFactor;
    owner->deliverGoods(output, _cropsFrom3 * harvest); 
    fields[Ripe3] -= harvest;
    fields[Ended] += harvest;
    
    harvest = min(fields[Ripe2], (int) floor(availableLabour / (_labourToReap * capFactor)));
    availableLabour -= harvest * _labourToReap * capFactor;
    owner->deliverGoods(output, _cropsFrom2 * harvest); 
    fields[Ripe2] -= harvest;
    fields[Ended] += harvest;
    
    harvest = min(fields[Ripe1], (int) floor(availableLabour / (_labourToReap * capFactor)));
    availableLabour -= harvest * _labourToReap * capFactor;
    owner->deliverGoods(output, _cropsFrom1 * harvest);
    fields[Ripe1] -= harvest;
    fields[Ended] += harvest;

    break;
  }
  } // Not a typo, ends switch.

  double usedLabour = availableLabour - getAmount(TradeGood::Labor);
  deliverGoods(TradeGood::Labor, usedLabour);
  if (getAmount(TradeGood::Labor) < 0) throw string("Negative labour after workFields");
}

void Farmland::countTotals () {
  totalFields[Clear] = farmers[0]->fields[Clear];
  totalFields[Ready] = farmers[0]->fields[Ready];
  totalFields[Sowed] = farmers[0]->fields[Sowed];
  totalFields[Ripe1] = farmers[0]->fields[Ripe1];
  totalFields[Ripe2] = farmers[0]->fields[Ripe2];
  totalFields[Ripe3] = farmers[0]->fields[Ripe3];
  totalFields[Ended] = farmers[0]->fields[Ended];
  
  for (int i = 1; i < numOwners; ++i) {
    totalFields[Clear] += farmers[i]->fields[Clear];
    totalFields[Ready] += farmers[i]->fields[Ready];
    totalFields[Sowed] += farmers[i]->fields[Sowed];
    totalFields[Ripe1] += farmers[i]->fields[Ripe1];
    totalFields[Ripe2] += farmers[i]->fields[Ripe2];
    totalFields[Ripe3] += farmers[i]->fields[Ripe3];
    totalFields[Ended] += farmers[i]->fields[Ended];
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
