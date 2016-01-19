#include "Building.hh"
#include "Hex.hh"
#include "GraphicsInfo.hh"
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

vector<const FieldStatus*> FieldStatus::plowing;
vector<const FieldStatus*> FieldStatus::weeding;
vector<const FieldStatus*> FieldStatus::harvest;

int Farmer::_labourToClear  = 100;
int Forest::_labourToClear   = 250;
int Mine::_amountOfIron = 50;

char errorMessage[500];

Castle::Castle (Hex* dat, Line* lin) 
  : Mirrorable<Castle>()
  , GraphicsBridge<Castle, CastleGraphicsInfo>(this)
  , support(dat)
  , location(lin)
  , recruitType(0) 
{
  recruitType = *(MilUnitTemplate::start());
  setMirrorState();
  support->getMarket()->registerParticipant(this);
}

Castle::Castle (Castle* other)
  : Mirrorable<Castle>(other)
  , GraphicsBridge<Castle, CastleGraphicsInfo>()
  , support(0) // Sequence issues - this constructor is called before anything is initialised in real constructor
{}

Castle::~Castle () {
  for (std::vector<MilUnit*>::iterator i = garrison.begin(); i != garrison.end(); ++i) {
    (*i)->destroyIfReal();
  }
  garrison.clear();
  fieldForce.clear();
}

void Castle::addGarrison (MilUnit* p) {
  assert(p); 
  garrison.push_back(p);
  p->setCastle(this);
  p->setLocation(0);
  p->setExtMod(siegeModifier); // Fortification bonus
  p->leaveMarket();
  vector<MilUnit*>::iterator loc = find(fieldForce.begin(), fieldForce.end(), p);
  if (loc != fieldForce.end()) fieldForce.erase(loc);
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
  distributeSupplies();
}

void Castle::getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {
  vector<MarketBid*> unitBids;
  GoodsHolder allBids;
  orders.clear();
  double availableMoney = getAmount(TradeGood::Money);
  BOOST_FOREACH(MilUnit* mu, garrison) {
    unitBids.clear();
    mu->deliverGoods(TradeGood::Money, availableMoney);
    mu->getBids(prices, unitBids);
    mu->deliverGoods(TradeGood::Money, -availableMoney);
    BOOST_FOREACH(MarketBid* mb, unitBids) {
      orders[mu].deliverGoods(mb->tradeGood, mb->amountToBuy);
      allBids.deliverGoods(mb->tradeGood, mb->amountToBuy);
      delete mb;
    }
  }
  BOOST_FOREACH(MilUnit* mu, fieldForce) {
    unitBids.clear();
    mu->deliverGoods(TradeGood::Money, availableMoney);
    mu->getBids(prices, unitBids);
    mu->deliverGoods(TradeGood::Money, -availableMoney);
    BOOST_FOREACH(MarketBid* mb, unitBids) {
      orders[mu].deliverGoods(mb->tradeGood, mb->amountToBuy);
      allBids.deliverGoods(mb->tradeGood, mb->amountToBuy);
      delete mb;
    }  
  }

  double totalMoneyNeeded = allBids * prices;
  if (totalMoneyNeeded < 0.001) return;
  double ratio = getAmount(TradeGood::Money) / totalMoneyNeeded;
  if (ratio > 1) ratio = 1;

  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    double amount = allBids.getAmount(*tg) * ratio - getAmount(*tg);
    if (0.01 > amount) continue;
    bidlist.push_back(new MarketBid((*tg), amount, this, 1));
  }
}

void Castle::deliverToUnit (MilUnit* unit, const GoodsHolder& goods) {
  if (find(garrison.begin(), garrison.end(), unit) != garrison.end()) {
    // It's right here in the castle, just hand it over.
    unit->deliverGoods(goods);
    (*this) -= goods;
    return;
  }
  // Field force, more difficult.
  TransportUnit* tu = new TransportUnit(unit);
  tu->deliverGoods(goods);
  (*this) -= goods;
  tu->setLocation(getLocation()->oneEnd());
}

void Castle::distributeSupplies () {
  if (0 == orders.size()) return;
  GoodsHolder totalBids;
  for (map<MilUnit*, GoodsHolder>::const_iterator bid = orders.begin(); bid != orders.end(); ++bid) {
    totalBids += (*bid).second;
  }

  for (map<MilUnit*, GoodsHolder>::iterator bid = orders.begin(); bid != orders.end(); ++bid) {
    for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
      double amountWanted = (*bid).second.getAmount(*tg);
      double amountAvailable = getAmount(*tg);
      if (amountWanted <= amountAvailable) continue;
      amountWanted /= totalBids.getAmount(*tg);
      amountWanted *= amountAvailable;
      (*bid).second.setAmount((*tg), amountWanted);
    }
  }
  for (map<MilUnit*, GoodsHolder>::const_iterator bid = orders.begin(); bid != orders.end(); ++bid) {
    deliverToUnit((*bid).first, (*bid).second);
  }
}

MilUnit* Castle::removeGarrison () {
  if (0 == garrison.size()) return 0;
  MilUnit* ret = garrison.back();
  garrison.pop_back();
  fieldForce.push_back(ret);
  ret->setCastle(0);
  ret->dropExtMod(); // No longer gets fortification bonus. 
  return ret;
}

MilUnit* Castle::removeUnit (MilUnit* dat) {
  std::vector<MilUnit*>::iterator target = std::find(garrison.begin(), garrison.end(), dat);
  if (target == garrison.end()) return 0;
  MilUnit* ret = (*target);
  garrison.erase(target);
  ret->setCastle(0);
  fieldForce.push_back(ret);
  ret->dropExtMod(); // No longer gets fortification bonus. 
  return ret; 
}

void Castle::recruit (Outcome out) {
  if (!recruitType) recruitType = *(MilUnitTemplate::start());
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

void Castle::unitTests () {
  Hex* testHex = Hex::getTestHex();
  Castle* testCastle = new Castle(testHex, *(testHex->linBegin()));
  MilUnit* garrison = MilUnit::getTestUnit();
  testCastle->addGarrison(garrison);
  testCastle->setAmount(TradeGood::Money, 1e6);
  GoodsHolder prices;
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) prices.setAmount((*tg), 1);
  vector<MarketBid*> bidlist;
  testCastle->getBids(prices, bidlist);
  if (0 == bidlist.size()) throwFormatted("Expected garrisoned Castle to make bids, got none");

  unsigned int initialUnits = TransportUnit::totalAmount();
  BOOST_FOREACH(MarketBid* mb, bidlist) testCastle->deliverGoods(mb->tradeGood, mb->amountToBuy);
  testCastle->distributeSupplies();
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (fabs(garrison->getAmount(mb->tradeGood) - mb->amountToBuy) > 0.01) throwFormatted("Expected garrison unit to get %.2f %s, but got %.2f",
											  mb->amountToBuy,
											  mb->tradeGood->getName().c_str(),
											  garrison->getAmount(mb->tradeGood));
  }

  if (initialUnits != TransportUnit::totalAmount()) throwFormatted("Expected %i TransportUnits, found %i", initialUnits, TransportUnit::totalAmount());
  testCastle->removeGarrison();
  garrison->setLocation(testCastle->getLocation()->twoEnd());
  garrison->zeroGoods();
  BOOST_FOREACH(MarketBid* mb, bidlist) testCastle->deliverGoods(mb->tradeGood, mb->amountToBuy);
  testCastle->distributeSupplies();
  if (initialUnits + 1 != TransportUnit::totalAmount()) throwFormatted("Expected Castle to create one TransportUnit, found %i", TransportUnit::totalAmount());
  TransportUnit::Iter tu = TransportUnit::start();
  (*tu)->endOfTurn();
  TransportUnit::cleanUp();
  if (initialUnits != TransportUnit::totalAmount()) throwFormatted("Expected TransportUnit to reach destination and destroy itself, %i -> %i",
								   initialUnits,
								   TransportUnit::totalAmount());
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (fabs(garrison->getAmount(mb->tradeGood) - mb->amountToBuy) > 0.01) throwFormatted("Expected field unit to get %.2f %s, but got %.2f (%i -> %i)",
											  mb->amountToBuy,
											  mb->tradeGood->getName().c_str(),
											  garrison->getAmount(mb->tradeGood),
											  initialUnits,
											  TransportUnit::totalAmount());
  }

  delete testCastle;
  delete testHex;
  delete garrison;
}

void Building::setOwner (Player* p) {
  owner = p;
}

void Castle::receiveTaxes (TradeGood const* const tg, double received) {
  EconActor::receiveTaxes(tg, received);
  if (canReport()) reportEvent(DisplayEvent("Received taxes",
					    createString("%.1f %s",
							 received,
							 tg->getName().c_str())));
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
  mirror->recruitType = recruitType;

  mirror->garrison.clear();  
  for (std::vector<MilUnit*>::iterator unt = garrison.begin(); unt != garrison.end(); ++unt) {
    (*unt)->setMirrorState(); 
    mirror->garrison.push_back((*unt)->getMirror());
  }
  mirror->setAmounts(this);
  setEconMirrorState(mirror);
}

Village::Village ()
  : Building()
  , Mirrorable<Village>()
  , GraphicsBridge<Village, VillageGraphicsInfo>(this)
  , milTrad(0)
  , farm(0)
  , consumptionLevel(maslowLevels[0])
  , workedThisTurn(0)
{
  milTrad = new MilitiaTradition();
  initialiseGraphicsBridge();
}

Village::Village (Village* other)
  : Building()
  , Mirrorable<Village>(other)
  , GraphicsBridge<Village, VillageGraphicsInfo>()
  , milTrad(0)
  , farm(0)
  , consumptionLevel(maslowLevels[0])
  , workedThisTurn(0)
{}

Village::~Village () {
  if (milTrad) milTrad->destroyIfReal();
}

void Village::MaslowLevel::normalise () {
  double total = 0;
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    total += getAmount(*tg);
  }
  if (0 == total) throwFormatted("Maslow level %s has zero defined goods", name.c_str());
  setAmount(TradeGood::Money, total);
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    double amount = getAmount(*tg);
    amount /= total;
    setAmount((*tg), amount);
  }
}

void Village::getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {
  // For each level in the Maslow hierarchy,
  // see if we can sell enough labor to cover it
  // (at the given prices). Then put in a sell bid
  // for enough labor to buy the last level we can
  // completely cover, and buy bids to get us that
  // level.
  GoodsHolder resources(*this);
  double labourAvailable = production();
  resources.deliverGoods(TradeGood::Labor, labourAvailable);
  resources -= promisedToDeliver;
  resources.setAmount(TradeGood::Money, 0.5 * getAmount(TradeGood::Money));
  double inverseTotalLabour = 1.0 / labourAvailable;
  double consumptionFactor  = consumption();

  GoodsHolder reserveUsed;
  GoodsHolder totalToBuy;
  expectedConsumptionLevel = 0;
  stopReason = "max consumption";
  BOOST_FOREACH(MaslowLevel const* level, maslowLevels) {
    bool canGetLevel = true;
    GoodsHolder amountAfterSubstitution;
    GoodsHolder effectivePrices;
    int numGoods = 0;
    double weightedProduct = 1;
    TradeGood::Iter firstProduct = TradeGood::final();
    vector<const TradeGood*> others;
    double totalConsumption = consumptionFactor * level->getAmount(TradeGood::Money);
    for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
      double weight = level->getAmount(*tg);
      if (weight < 0.0001) continue;
      ++numGoods;
      double effPrice = prices.getAmount(*tg) / weight;
      effectivePrices.setAmount((*tg), effPrice);
      weightedProduct *= pow(effPrice, weight);
      if (firstProduct == TradeGood::final()) firstProduct = tg;
      else others.push_back(*tg);
    }
    if (1 == numGoods) {
      amountAfterSubstitution.setAmount((*firstProduct), totalConsumption);
    }
    else {
      // See the internaldocs section on substitutions for the math.
      double amountOfFirst = totalConsumption * weightedProduct;
      amountAfterSubstitution.setAmount((*firstProduct), amountOfFirst / effectivePrices.getAmount(*firstProduct));
      for (vector<const TradeGood*>::const_iterator tg = others.begin(); tg != others.end(); ++tg) {
	double amountOfThis = amountOfFirst / effectivePrices.getAmount(*tg);
	amountAfterSubstitution.setAmount((*tg), amountOfThis);
      }
    }

    GoodsHolder amountToBuy;
    double labourThisLevel = 0;

    for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
      double amountNeeded = amountAfterSubstitution.getAmount(*tg);
      if (amountNeeded < resources.getAmount(*tg)) {
	resources.deliverGoods((*tg), -amountNeeded);
	continue;
      }
      else {
	amountNeeded -= resources.getAmount(*tg);
	resources.setAmount((*tg), 0);
      }

      // Use labour before cash to avoid too-wealthy-to-work problem.
      double totalPrice = amountNeeded * prices.getAmount(*tg);
      double labourNeeded = totalPrice / prices.getAmount(TradeGood::Labor);
      if (labourNeeded > resources.getAmount(TradeGood::Labor)) {
	stopReason = "not enough labour for " + (*tg)->getName();
      }
      else if ((labourThisLevel + labourNeeded) * inverseTotalLabour > level->maxWorkFraction) {
	stopReason = "too much work for " + (*tg)->getName();
      }
      else {
	labourThisLevel += labourNeeded;
	resources.deliverGoods(TradeGood::Labor, -labourNeeded);
	amountToBuy.deliverGoods((*tg), amountNeeded);
	continue;
      }

      // Can't or won't work for it; try cash.
      if (totalPrice < resources.getAmount(TradeGood::Money)) {
	amountToBuy.deliverGoods((*tg), amountNeeded);
	resources.deliverGoods(TradeGood::Money, -totalPrice);
      }
      else {
	canGetLevel = false;
	stopReason += " and not enough cash";
      }
    }

    if (!canGetLevel) break;
    totalToBuy += amountToBuy;
    totalToBuy.deliverGoods(TradeGood::Labor, -labourThisLevel);
    expectedConsumptionLevel = level;
  }

  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    if (0.1 > fabs(totalToBuy.getAmount(*tg))) continue;
    bidlist.push_back(new MarketBid((*tg), totalToBuy.getAmount(*tg), this));
  }
}

Village* Village::getTestVillage (int pop) {
  Village* testVillage = new Village();
  testVillage->males.addPop(pop, 20);
  return testVillage;
}

void Village::unitTests () {
  Hex* testHex = Hex::getTestHex(true, false, false, false);
  Village* testVillage = testHex->getVillage();
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
  BOOST_FOREACH(MaslowLevel* ml, maslowLevels) ml->normalise();
  prices.zeroGoods();
  prices.setAmount(TradeGood::Labor, 1);
  prices.setAmount(theGood, 5);
  // Buying 0.2 food now costs 1 labour. The village should be unwilling
  // to make that trade because the maximum for the second level is 0.45.
  // It should still buy the first level, however.
  bidlist.clear();
  testVillage->getBids(prices, bidlist);
  if (2 != bidlist.size()) throwFormatted("Line %i: Expected 2 bids, got %i, reason %s", __LINE__, bidlist.size(), testVillage->stopReason.c_str());
  GoodsHolder theBids;
  BOOST_FOREACH(MarketBid* mb, bidlist) theBids.deliverGoods(mb->tradeGood, mb->amountToBuy);
  // Minus one from selling
  double labourExpected = -1 * (maslowLevels[0]->getAmount(TradeGood::Money) * testVillage->consumption() * prices.getAmount(theGood)) / prices.getAmount(TradeGood::Labor);
  double labourBought = theBids.getAmount(TradeGood::Labor);
  if (0.001 < fabs(labourExpected - labourBought)) throwFormatted("Line %i: Expected to sell %f labour, but sold %f, reason %s",
								  __LINE__,
								  -labourExpected,
								  -labourBought,
								  testVillage->stopReason.c_str());
  double foodExpected = maslowLevels[0]->getAmount(TradeGood::Money) * testVillage->consumption();
  double foodBought = theBids.getAmount(theGood);
  if (0.001 < fabs(foodExpected - foodBought)) throwFormatted("Line %i: Expected to buy %f %s, but bought %f, reason %s",
							      __LINE__,
							      foodExpected,
							      theGood->getName().c_str(),
							      foodBought,
							      testVillage->stopReason.c_str());

  prices.setAmount(TradeGood::Labor, 2);
  // Now the full trade, both levels, should be accepted.
  bidlist.clear();
  testVillage->getBids(prices, bidlist);
  if (2 != bidlist.size()) throwFormatted("Line %i: Expected 2 bids, got %i, reason %s", __LINE__, bidlist.size(), testVillage->stopReason.c_str());
  labourBought = 0;
  BOOST_FOREACH(MarketBid* mb, bidlist) if (mb->tradeGood == TradeGood::Labor) labourBought += mb->amountToBuy;
  labourExpected = -1 * ((maslowLevels[0]->getAmount(TradeGood::Money) + maslowLevels[1]->getAmount(TradeGood::Money)) * testVillage->consumption() * prices.getAmount(theGood)) / prices.getAmount(TradeGood::Labor); 
  if (0.001 < fabs(labourExpected - labourBought)) throwFormatted("Line %i: Expected to buy %f labour, but bought %f, reason %s",
								  __LINE__,
								  labourExpected,
								  labourBought,
								  testVillage->stopReason.c_str());

  // Substitution test. With a cheap second good in the level,
  // the village should buy a lot of that.
  TradeGood const* nextGood = *(TradeGood::exLaborStart()+1);
  maslowLevels[0]->setAmount(theGood, 0.1);
  maslowLevels[0]->setAmount(nextGood, 0.1);
  BOOST_FOREACH(MaslowLevel* ml, maslowLevels) ml->normalise();
  prices.setAmount(nextGood, 0.1); // Tiny price so it can easily buy
  bidlist.clear();
  testVillage->getBids(prices, bidlist);
  if (3 != bidlist.size()) throwFormatted("Line %i: Expected 3 bids, got %i, reason %s", __LINE__, bidlist.size(), testVillage->stopReason.c_str());
  theBids.zeroGoods();
  BOOST_FOREACH(MarketBid* mb, bidlist) theBids.deliverGoods(mb->tradeGood, mb->amountToBuy);
  double woodExpected = testVillage->consumption();
  woodExpected *= pow(prices.getAmount(theGood) / maslowLevels[0]->getAmount(theGood), maslowLevels[0]->getAmount(theGood));
  woodExpected *= pow(prices.getAmount(nextGood) / maslowLevels[0]->getAmount(nextGood), maslowLevels[0]->getAmount(nextGood));
  double woodBought = theBids.getAmount(nextGood);
  if (0.001 < fabs(woodExpected - woodBought)) throwFormatted("Line %i: Expected to buy %f %s, but bought %f, reason %s",
							      __LINE__,
							      woodExpected,
							      nextGood->getName().c_str(),
							      woodBought,
							      testVillage->stopReason.c_str());

  // Jack up price of second good so it will stop from "too much work".
  prices.setAmount(nextGood, 10.0);
  bidlist.clear();
  testVillage->getBids(prices, bidlist);
  if (0 != bidlist.size()) throwFormatted("Expected 0 bids (fourth time), got %i, reason %s", bidlist.size(), testVillage->stopReason.c_str());
  sprintf(errorMessage, "too much work for %s and not enough cash", nextGood->getName().c_str());
  if (testVillage->stopReason != errorMessage) throwFormatted("Expected too much work, got %s", testVillage->stopReason.c_str());

  // Check that the consumption is correct.
  maslowLevels.pop_back();
  for (int ratio = 1; ratio < 6; ++ratio) {
    testVillage->zeroGoods();
    testVillage->setAmount(theGood, ratio*testVillage->consumption());
    testVillage->setAmount(nextGood, testVillage->consumption());
    testVillage->eatFood();
    if (testVillage->consumptionLevel != maslowLevels[0]) throwFormatted("%s:%i Expected to reach maslow level 0", __FILE__, __LINE__);
    double goodConsumed = ratio*testVillage->consumption() - testVillage->getAmount(theGood);
    double nextConsumed = testVillage->consumption() - testVillage->getAmount(nextGood);
    goodConsumed /= theGood->getConsumption();
    nextConsumed /= nextGood->getConsumption();
    if (0.1 < fabs(goodConsumed - ratio*nextConsumed))
      throwFormatted("%s:%i Expected to consume %i times as much %s as %s, but consumed %.2f and %.2f",
		     __FILE__,
		     __LINE__,
		     ratio,
		     theGood->getName().c_str(),
		     nextGood->getName().c_str(),
		     goodConsumed,
		     nextConsumed);
  }

  consume[20] = oldConsume;
  delete testVillage;

  testVillage = getTestVillage(100);
  testVillage->zeroGoods();
  double production = testVillage->production();
  ContractInfo labourContract;
  labourContract.source = testVillage;
  labourContract.recipient = testVillage;
  labourContract.amount = 0.25;
  labourContract.delivery = ContractInfo::Percentage;
  labourContract.tradeGood = TradeGood::Labor;
  labourContract.execute();
  if (fabs(testVillage->getAmount(TradeGood::Labor) - production*labourContract.amount) > 1) throwFormatted("Expected to get %.2f labour from percentage, but got %.2f",
													    production*labourContract.amount,
													    testVillage->getAmount(TradeGood::Labor));
  testVillage->zeroGoods();
  labourContract.amount = 10;
  labourContract.delivery = ContractInfo::Fixed;
  labourContract.execute();
  if (fabs(testVillage->getAmount(TradeGood::Labor) - labourContract.amount) > 1) throwFormatted("Expected to get %.2f labour from fixed tax, but got %.2f",
												 labourContract.amount,
												 testVillage->getAmount(TradeGood::Labor));

  maslowLevels = backupLevels;
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

string MilitiaTradition::display () const {
  string ret = "";
  for (map<MilUnitTemplate const* const, int>::const_iterator i = militiaStrength.begin(); i != militiaStrength.end(); ++i) {
    if (0 == (*i).second) continue;
    ret += createString("\n%s: %i", (*i).first->getName().c_str(), (*i).second);
  }
  return ret;
}

void MilitiaTradition::setMirrorState () {
  militia->setMirrorState();  
  mirror->militia = militia->getMirror();
  mirror->militiaStrength.clear();
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
    deliverGoods(tg, amount);
  }
  return EconActor::produceForContract(tg, amount);
}

double Village::produceForTaxes (TradeGood const* const tg, double amount, ContractInfo::AmountType taxType) {
  double ret = amount;
  double initialAmount = getAmount(*tg);
  if (tg == TradeGood::Labor) {
    initialAmount = production();
    if (ContractInfo::Percentage == taxType) ret *= initialAmount;
    ret = min(ret, initialAmount);
    if (ret < 0) throw string("Cannot work more than production");
    workedThisTurn += ret;
  }
  else {
    ret = EconActor::produceForTaxes(tg, amount, taxType);
  }
  if ((isReal()) && (getGraphicsInfo())) {
    getGraphicsInfo()->addEvent(DisplayEvent("Paid taxes",
					     createString("%.1f of %.1f (required %.1f) %s",
							  ret,
							  initialAmount,
							  amount * initialAmount,
							  tg->getName().c_str())));
  }
  return ret;
}

void Village::endOfTurn () {
  eatFood();
  workedThisTurn = 0;

  int deaths = 0;
  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    double fracLoss = adjustedMortality(i, true) * males.getPop(i);
    int die = convertFractionToInt(fracLoss);
    deaths += die;
    males.addPop(-die, i);

    fracLoss = adjustedMortality(i, false) * women.getPop(i);
    die = convertFractionToInt(fracLoss);
    deaths += die;
    women.addPop(-die, i);
  }
 
  // Simplifying assumptions:
  // No man ever impregnates an older woman
  // Male fertility is constant, only female fertility matters
  // No man gets more than one woman pregnant in a year 
  // Young men get the first chance at single women

  int popIncrease = 0;
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
  if ((isReal()) && (getGraphicsInfo())) {
    getGraphicsInfo()->addEvent(DisplayEvent(createString("%i births, %i deaths", popIncrease, deaths), ""));
    if (0.1 < milTrad->getRequiredWork()) getGraphicsInfo()->addEvent(DisplayEvent(createString("Drilled militia (%.1f labour)", milTrad->getRequiredWork()),
										   createString("Militia strength:%s", milTrad->display().c_str())));
    if (1 < getSold(TradeGood::Labor)) getGraphicsInfo()->addEvent(DisplayEvent("Worked for wages",
										createString("Sold:%s\nIncome:%s",
											     soldThisTurn.display().c_str(),
											     earnedThisTurn.display().c_str())));
  }

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

  milTrad->increaseTradition();
  return milTrad->militia;
}

Farmland::Farmland ()
  : Building(1, 5, 1e6)
  , Mirrorable<Farmland>()
  , Collective<Farmer, FieldStatus, 10>()
  , totalFields(FieldStatus::numTypes(), 0)
{
  createWorkers(this);
}

Farmland::~Farmland () {}

Farmland::Farmland (Farmland* other)
  : Building(other->marginFactor, other->blockSize, other->workableBlocks)
  , Mirrorable<Farmland>(other)
{}

Farmer::Farmer (Farmland* b)
  : Industry<Farmer>(this, b)
  , Mirrorable<Farmer>()
  , fields(FieldStatus::numTypes(), 0)
  , extraLabour(0)
  , totalWorked(0)
{}

Farmer::Farmer (Farmer* other)
  : Industry<Farmer>(this, other->blockInfo)
  , Mirrorable<Farmer>(other)
  , fields(FieldStatus::numTypes(), 0)
  , extraLabour(other->extraLabour)
  , totalWorked(other->totalWorked)
{}

void Farmer::setMirrorState () {
  for (unsigned int i = 0; i < fields.size(); ++i) mirror->fields[i] = fields[i];
  setEconMirrorState(mirror);
  mirror->extraLabour = extraLabour;
  mirror->totalWorked = totalWorked;
}

Farmer::~Farmer () {}

double Farmer::outputOfBlock (int /*block*/) const {
  // May be inaccurate for the last block. TODO: It would be good to have
  // an expectation value instead, with a discount rate and accounting for
  // the current state of the block.
  return (*FieldStatus::startReap())->yield * blockInfo->blockSize;
}

Farmland* Farmland::getTestFarm (int numFields) {
  Farmland* testFarm = new Farmland();
  testFarm->workers[0]->fields[**FieldStatus::start()] = numFields;
  return testFarm;
}

int Farmland::getTotalFields () const {
  int ret = 0;
  for (FieldStatus::Iter fs = FieldStatus::start(); fs != FieldStatus::final(); ++fs) {
    ret += totalFields[**fs];
  }
  return ret;
}

void Farmland::unitTests () {
  Hex* testHex = Hex::getTestHex(false, true, false, false);
  Farmland* testFarm = testHex->getFarm();
  // Stupidest imaginable test, but did actually catch a problem, so...
  if ((int) testFarm->workers.size() != numOwners) {
    sprintf(errorMessage, "Farm should have %i Farmers, has %i", numOwners, testFarm->workers.size());
    throw string(errorMessage);
  }

  Farmer* testFarmer = testFarm->workers[0];
  testFarmer->unitTests();
  delete testFarm;
  if (testFarmer->theMarket) throw string("Expected farmer to have been unregistered");
}

int Farmer::numBlocks () const {
  int total = 0;
  BOOST_FOREACH(int f, fields) total += f;
  int blocks = total / blockInfo->blockSize;
  if (blocks * blockInfo->blockSize < total) ++blocks;
  return blocks;
}

string Farmer::writeFieldStatus () const {
  FieldStatus::Iter fs = FieldStatus::start();
  sprintf(errorMessage, "%s %i", (*fs)->getName().c_str(), fields[**fs]);
  for (++fs; fs != FieldStatus::final(); ++fs) {
    sprintf(errorMessage, "%s, %s %i", errorMessage, (*fs)->getName().c_str(), fields[**fs]);
  }
  return string(errorMessage);
}

void Farmer::unitTests () {
  if (!theMarket) throw string("Farm has not been registered with a market.");
  if (!output) throw string("Farm output has not been set.");
  if (0 >= blockInfo->blockSize) throw new string("Farmer expected positive block size");
  // Note that this is not static, it's run on a particular Farmer.
  GoodsHolder oldCapital(*capital);
  capital->zeroGoods();
  TradeGood const* testCapGood = 0;
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    if ((*tg) == output) continue;
    testCapGood = (*tg);
    capital->setAmount((*tg), 0.1);
    break;
  }

  jobInfo testJob(1.0, 1, 1);
  testJob.numChunks() += 1;
  if (2 != testJob.numChunks()) throwFormatted("Chunks should now be 2, got %i", testJob.numChunks());
  vector<jobInfo> testJobs;
  testJobs.push_back(testJob);
  BOOST_FOREACH(jobInfo& job, testJobs) {
    if (2 != job.numChunks()) throwFormatted("Got 2 chunks a moment ago, now %i", job.numChunks());
    job.numChunks() += 1;
    if (3 != job.numChunks()) throwFormatted("Chunks should now be 3, got %i", job.numChunks());
  }
  searchForMatch(testJobs, 1.0, 1, 1);
  if (4 != testJobs.back().numChunks()) throwFormatted("Chunks should now be 4, got %i", testJobs.back().numChunks());

  Calendar::newYearBegins();
  fields[**FieldStatus::start()] = 1400;
  double fullCycleLabour;
  vector<jobInfo> jobs;
  double laborNeeded = 0;
  getLabourForBlock(0, jobs, laborNeeded);
  if (0 == jobs.size()) throwFormatted("Expected to find some jobs, got 0");
  if (laborNeeded <= 0) throwFormatted("Expected to need positive amount of labor, but found %f", laborNeeded);

  deliverGoods(testCapGood, 1);
  double laborNeededWithCapital = 0;
  getLabourForBlock(0, jobs, laborNeededWithCapital);
  if (0 == jobs.size()) throwFormatted("Expected to find some jobs with capital, got 0");
  if (laborNeededWithCapital >= laborNeeded) throwFormatted("With capital %f of %s, should require less than %f labour, but need %f",
							    getAmount(testCapGood),
							    testCapGood->getName().c_str(),
							    laborNeeded,
							    laborNeededWithCapital);
  deliverGoods(testCapGood, -1);

  GoodsHolder prices;
  vector<MarketBid*> bidlist;
  double foundLabour  = 0;
  double foundCapGood = 0;
  double foundOutput  = 0;

  // Check that we bid for integer numbers of fields.
  prices.setAmount(TradeGood::Labor, 10);
  prices.setAmount(testCapGood, 100000);
  prices.setAmount(output, 100000);
  getBids(prices, bidlist);
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) foundLabour += mb->amountToBuy;
  }
  double springLabour = 0;
  FieldStatus::Iter fs = FieldStatus::startPlow();
  double winterLabour = (*fs)->winterLabour;;
  for (; fs != FieldStatus::finalPlow(); ++fs) springLabour += (*fs)->springLabour;
  double expected = fields[**FieldStatus::startPlow()] * (winterLabour + springLabour / Calendar::turnsToNextSeason());
  if (fabs(foundLabour - expected) > 0.1) throwFormatted("Expected to buy %i * (%.2f + %.2f / %i) = %.2f, but got %.2f",
							 fields[**FieldStatus::startPlow()],
							 winterLabour,
							 springLabour,
							 Calendar::turnsToNextSeason(),
							 expected,
							 foundLabour);
  bidlist.clear();

  // Expect to produce more than one output per labour, so with equal prices
  // we should buy some. Cheap capital should be a net saving.
  prices.setAmount(TradeGood::Labor, 10);
  prices.setAmount(testCapGood, 1);
  prices.setAmount(output, 10);
  deliverGoods(output, 100);
  getBids(prices, bidlist);

  foundLabour = 0;
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
  if (foundLabour <= 0) throwFormatted("Expected to buy %s, found %f", TradeGood::Labor->getName().c_str(), foundLabour);
  if (foundCapGood <= 0) throwFormatted("Expected to buy capital %s, found %f", testCapGood->getName().c_str(), foundCapGood);
  if (foundOutput >= -1) throwFormatted("Expected to sell at least one %s, found %f", output->getName().c_str(), foundOutput);
  // 7.143 is three forty-twoths of the reserve of 100 - the amount we approach asymptotically as profit goes to infinity.
  if (foundOutput < -7.143) throwFormatted("Did not expect to sell more than 7.143 units, found %f", foundOutput);

  // Reduce the profit, check that we sell less.
  prices.setAmount(TradeGood::Labor, 11);
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
  if (foundLabour <= 0) throwFormatted("Expected (again) to buy %s, found %.2f", TradeGood::Labor->getName().c_str(), foundLabour);
  if (foundCapGood <= 0) throwFormatted("Expected (again) to buy %s, found %.2f", testCapGood->getName().c_str(), foundCapGood);
  if (fabs(secondFoundOutput) >= fabs(foundOutput)) throwFormatted("Profit decrease should lead to lower bid offer, found %f -> %f", foundOutput, secondFoundOutput);
  
  deliverGoods(output, -100);

  owner = this;
  int canonicalBlocks = numBlocks();
  while (Calendar::getCurrentSeason() != Calendar::Winter) {
    int currentBlocks = numBlocks();
    if (currentBlocks != canonicalBlocks)
      throwFormatted("Expected to have %i blocks, but have %i. %s",
		     canonicalBlocks,
		     currentBlocks,
		     writeFieldStatus().c_str());
    laborNeeded = 0;
    jobs.clear();
    for (int i = 0; i < currentBlocks; ++i) getLabourForBlock(i, jobs, laborNeeded);
    BOOST_FOREACH(jobInfo job, jobs) deliverGoods(TradeGood::Labor, job.totalLabour());
    extractResources();
    Calendar::newWeekBegins();
  }
  FieldStatus::rIter endStatus = FieldStatus::rstart();
  if (fields[**endStatus] != 1400)
    throwFormatted("Expected to have 1400 %s fields, but have %i. %s",
		   (*endStatus)->getName().c_str(),
		   fields[**endStatus],
		   writeFieldStatus().c_str());

  FieldStatus::Iter ripeStatus = FieldStatus::startReap();
  double expectedFood = (*ripeStatus)->yield*1400 * extraLabourFactor(extraLabour, totalWorked);
  if (fabs(getAmount(output) - expectedFood) > 1)
    throwFormatted("Expected to have %.2f %s, but have %.2f; extra %.2f, total %.2f, %s",
		   expectedFood,
		   output->getName().c_str(),
		   getAmount(output),
		   extraLabour,
		   totalWorked,
		   writeFieldStatus().c_str());

  blockInfo->blockSize = 5;
  fields[**endStatus] = 6;
  vector<int> theBlock(FieldStatus::numTypes(), 0);
  fillBlock(0, theBlock);
  if (theBlock[**endStatus] != blockInfo->blockSize) throwFormatted("Expected %i %s fields in block 0, got %i",
								    blockInfo->blockSize,
								    (*endStatus)->getName().c_str(),
								    theBlock[**endStatus]);
  
  theBlock[**endStatus] = 0;
  fields[**endStatus] = 100;
  fillBlock(5, theBlock);
  if (theBlock[**endStatus] != blockInfo->blockSize) throwFormatted("Expected %i %s fields in block 5, got %i",
								    blockInfo->blockSize,
								    (*endStatus)->getName().c_str(),
								    theBlock[**endStatus]);
  
  theBlock[**endStatus] = 0;
  fields[**endStatus] = 1;
  fillBlock(1, theBlock);
  if (theBlock[**endStatus] != 0) throwFormatted("Expected 0 %s fields in block 1, got %i",
						 (*endStatus)->getName().c_str(),
						 theBlock[**endStatus]);

  theBlock[**endStatus] = 0;
  fields[**endStatus] = blockInfo->blockSize - 1;
  fields[**ripeStatus] = 1;
  fillBlock(0, theBlock);
  if (theBlock[**endStatus] != blockInfo->blockSize - 1) throwFormatted("Expected %i %s fields in block 0, got %i",
									blockInfo->blockSize - 1,
									(*endStatus)->getName().c_str(),
									theBlock[**endStatus]);
  if (theBlock[**ripeStatus] != 1) throwFormatted("Expected 1 %s fields in block 0, got %i",
						  (*ripeStatus)->getName().c_str(),
						  theBlock[**ripeStatus]);

  theBlock[**endStatus] = 0;
  theBlock[**ripeStatus] = 0;
  FieldStatus::Iter ripe2 = ripeStatus; ++ripe2;
  fields[**ripe2] = blockInfo->blockSize;
  fillBlock(1, theBlock);
  if (theBlock[**endStatus] != 0) throwFormatted("Did not expect %s fields in block 1", (*endStatus)->getName().c_str());
  if (theBlock[**ripeStatus] != 0) throwFormatted("Did not expect %s fields in block 1", (*ripeStatus)->getName().c_str());
  if (theBlock[**ripe2] != blockInfo->blockSize) throwFormatted("Expected %i %s fields in block 1, got %i",
								blockInfo->blockSize,
								(*ripe2)->getName().c_str(),
								theBlock[**ripe2]);

  Calendar::setWeek(30);
  if (Calendar::Autumn != Calendar::getCurrentSeason()) throwFormatted("Expected week 30 to be Autumn");
  for (unsigned int i = 0; i < fields.size(); ++i) fields[i] = 0;
  extraLabour = 0;
  totalWorked = 0;

  setAmount(output, 0);
  fields[**ripeStatus] = blockInfo->blockSize;
  getLabourForBlock(0, jobs, fullCycleLabour);
  setAmount(TradeGood::Labor, fullCycleLabour);
  extractResources();
  double firstOutput = getAmount(output);
  if (0.1 > firstOutput) throwFormatted("Expected output larger than 0.1");

  vector<double> marginFactors;
  marginFactors.push_back(0.99);
  marginFactors.push_back(0.9);
  marginFactors.push_back(0.1);
  BOOST_FOREACH(double mf, marginFactors) {
    blockInfo->marginFactor = mf;
    for (int blocks = 2; blocks < 5; ++blocks) {
      extraLabour = 0;
      totalWorked = 0;
      fields[**endStatus] = 0;
      fields[**ripeStatus] = blocks*blockInfo->blockSize;
      double gameExpected = 0;
      for (int j = 0; j < blocks; ++j) gameExpected += outputOfBlock(j) * pow(mf, j);
      setAmount(output, 0);
      getLabourForBlock(0, jobs, fullCycleLabour);
      setAmount(TradeGood::Labor, blocks * fullCycleLabour);
      extractResources();
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
  capital->setAmounts(oldCapital);
}

void Farmer::extractResources (bool /* tick */) {
  Calendar::Season currSeason = Calendar::getCurrentSeason();
  double availableLabour = getAmount(TradeGood::Labor);
  double capFactor = capitalFactor(*this);
  double totalHarvested = 0;
  const FieldStatus* endStatus = *(FieldStatus::rstart());
  switch (currSeason) {
  default:
  case Calendar::Winter:
    // Cleanup.
    // Used land becomes 'clear' for next year.
    for (FieldStatus::Iter fs = FieldStatus::startReap(); fs != FieldStatus::finalReap(); ++fs) {
      fields[*endStatus] += fields[**fs];
      fields[**fs] = 0;
    }

    // Fallow acreage grows over.
    fields[**(FieldStatus::start())] = fields[*endStatus];
    fields[*endStatus] = 0;
    extraLabour = 0;
    totalWorked = 0;
    break;

  case Calendar::Spring:
    // In spring we clear new fields, plow and sow existing ones.
    for (FieldStatus::Iter fs = FieldStatus::startPlow(); fs != FieldStatus::finalPlow(); ++fs) {
      if (1 > fields[**fs]) continue;
      double workPerField = (*fs)->springLabour*capFactor;
      if (availableLabour >= workPerField) {
	int numToAdvance  = min(fields[**fs], (int) floor(availableLabour / workPerField));
	fields[**fs]     -= numToAdvance;
	fields[**fs + 1] += numToAdvance;
	availableLabour  -= numToAdvance * workPerField;
      }
    }
    // TODO: Clearing of new land
    break;

  case Calendar::Summer: {
    // In summer the crops ripen and we fight the weeds.
    // If there isn't enough labour to tend the crops, they
    // degrade; if the weather is bad they don't advance.
    double weatherModifier = 1; // TODO: Insert weather-getting code here
    static vector<int> workingSpace(FieldStatus::numTypes());
    for (FieldStatus::Iter fs = FieldStatus::startWeed(); fs != FieldStatus::finalWeed(); ++fs) {
      workingSpace[**fs] = fields[**fs];
      fields[**fs] = 0;
    }

    FieldStatus::Iter prev = FieldStatus::startWeed();
    FieldStatus::Iter next = FieldStatus::startWeed(); ++next;

    for (FieldStatus::Iter curr = FieldStatus::startWeed(); curr != FieldStatus::finalWeed(); ++curr, ++prev, ++next) {
      if (next == FieldStatus::finalWeed()) next = curr;
      double labourPerField = (*curr)->summerLabour * capFactor * weatherModifier;
      int fieldsToWeed = min(workingSpace[**curr], (int) floor(availableLabour / labourPerField));
      fields[**next] += fieldsToWeed;
      fields[**prev] += workingSpace[**curr] - fieldsToWeed;
      availableLabour -= fieldsToWeed * labourPerField;
    }
    break;
  }
  case Calendar::Autumn: {
    // In autumn we harvest.
    int block = fields[*endStatus] / blockInfo->blockSize;
    int counter = fields[*endStatus] - block*blockInfo->blockSize;
    double marginFactor = pow(blockInfo->marginFactor, block);
    for (FieldStatus::Iter fs = FieldStatus::startReap(); fs != FieldStatus::finalReap(); ++fs) {
      if (1 > (*fs)->yield) continue;
      if (1 > fields[**fs]) continue;
      int harvest = min(fields[**fs], (int) floor(availableLabour / ((*fs)->autumnLabour * capFactor)));
      availableLabour -= harvest * (*fs)->autumnLabour * capFactor;
      for (int i = 0; i < harvest; ++i) {
	totalHarvested += (*fs)->yield * marginFactor;
	if (++counter >= blockInfo->blockSize) {
	  counter = 0;
	  marginFactor *= blockInfo->marginFactor;
	}
      }
      fields[**fs] -= harvest;
      fields[*endStatus] += harvest;
    }

    break;
  }
  } // Not a typo, ends switch.

  extraLabour += availableLabour;
  totalWorked -= (availableLabour - getAmount(TradeGood::Labor));
  if (totalHarvested > 0) { // This should occur after the last bits of extra and total labour have been recorded.
    totalHarvested *= extraLabourFactor(extraLabour, totalWorked);
    produce(output, totalHarvested);
  }

  setAmount(TradeGood::Labor, 0);
}

void Farmer::fillBlock (int block, vector<int>& theBlock) const {
  // Figure out the field status in the given block;
  // since block N is always worked on before block N+1,
  // we can do so by counting backwards from the top status
  // until we've reached block*blockSize fields.

  // Cache result for previous block to avoid Schlemiel when calling
  // successive blocks for the same Farmer.
  static const Farmer* lastFarmer = 0;
  static int lastBlock = 0;
  static int lastCounted = 0;
  static FieldStatus::rIter lastStatus = FieldStatus::rstart();
  
  int counted = 0;
  int found = 0;
  FieldStatus::rIter fs = FieldStatus::rstart();

  if ((lastFarmer == this) && (lastBlock + 1 == block)) {
    fs = lastStatus;
    counted = lastCounted;
  }

  for (; fs != FieldStatus::rfinal(); ++fs) {
    if (counted + fields[**fs] <= block * blockInfo->blockSize) {
      counted += fields[**fs];
      continue;
    }
    theBlock[**fs] = min(fields[**fs], blockInfo->blockSize - found);
    found += theBlock[**fs];
    if (found >= blockInfo->blockSize) break;
    counted += theBlock[**fs];
  }

  lastFarmer = this;
  lastBlock = block;
  lastCounted = counted;
  lastStatus = fs;
}

double Farmer::getCapitalSize () const {
  return numBlocks() * blockInfo->blockSize;
}

void Farmer::getLabourForBlock (int block, vector<jobInfo>& jobs, double& prodCycleLabour) const {
  // Returns the amount of labour needed to tend the
  // fields, on the assumption that the necessary labour
  // will be spread over the remaining turns in the season. 

  Calendar::Season currSeason = Calendar::getCurrentSeason();
  if (Calendar::Winter == currSeason) return;

  vector<int> theBlock(FieldStatus::numTypes(), 0);
  fillBlock(block, theBlock);

  double capFactor = capitalFactor(*this);
  prodCycleLabour = 0;
  switch (currSeason) {
  default:
  case Calendar::Spring: {
    // In spring we clear new fields, plow and sow existing ones.
    // Fields move from Clear to Ready to Sowed.
    int prev = 0;
    for (FieldStatus::Iter fs = FieldStatus::startPlow(); fs != FieldStatus::finalPlow(); ++fs) {
      searchForMatch(jobs, capFactor * (*fs)->springLabour, prev + theBlock[**fs], Calendar::turnsToNextSeason());
      prodCycleLabour += (prev + theBlock[**fs]) * (*fs)->springLabour;
      prev += theBlock[**fs];
    }

    FieldStatus::Iter sowed = FieldStatus::startWeed();
    FieldStatus::Iter riped = FieldStatus::startReap();
    // A full season of weeding - assume that all the fields will be sowed.
    prodCycleLabour += (prev + theBlock[**sowed]) * (*sowed)->summerLabour * Calendar::turnsPerSeason();
    // And assume that they all reach reaping.
    prodCycleLabour += (prev + theBlock[**riped]) * (*riped)->autumnLabour;
    break;
  }
  case Calendar::Summer:
    // Weeding is special: It is not done once and finished,
    // like plowing, sowing, and harvesting. It must be re-done
    // each turn.
    for (FieldStatus::Iter fs = FieldStatus::startWeed(); fs != FieldStatus::finalWeed(); ++fs) {
      searchForMatch(jobs, capFactor*(*fs)->summerLabour, theBlock[**fs], 1);
      prodCycleLabour += theBlock[**fs] * (*fs)->summerLabour * Calendar::turnsToNextSeason();
      prodCycleLabour += theBlock[**fs] * (*fs)->autumnLabour;
    }
    break;
      
  case Calendar::Autumn:
    for (FieldStatus::Iter fs = FieldStatus::startReap(); fs != FieldStatus::finalReap(); ++fs) {
      searchForMatch(jobs, capFactor * (*fs)->autumnLabour, theBlock[**fs], Calendar::turnsToNextSeason());
      prodCycleLabour += (*fs)->autumnLabour * theBlock[**fs];
    }
    break; 
  }

  prodCycleLabour *= capFactor;
}

double Farmer::getWinterLabour (const GoodsHolder& prices, int lastBlock, double expectedProd, double expectedLabour) const {
  expectedLabour += totalWorked;
  double maxPossible = 0;
  int currBlock = 0;
  int blockSize = blockInfo->blockSize;
  for (FieldStatus::Iter fs = FieldStatus::start(); fs != FieldStatus::final(); ++fs) {
    int numToUse = fields[**fs];
    int blocks = currBlock + numToUse/blockSize;
    if (blocks > lastBlock) {
      numToUse = (lastBlock - currBlock) / blockSize;
      blocks = lastBlock;
    }
    maxPossible += numToUse * (*fs)->winterLabour;
    currBlock = blocks;
    if (currBlock == lastBlock) break;
  }
  double currentExtraProduction = expectedProd * extraLabourFactor(extraLabour, expectedLabour);
  double lowestExtraProduction = expectedProd * extraLabourFactor(extraLabour + 1, expectedLabour) - currentExtraProduction;
  if (lowestExtraProduction * prices.getAmount(output) <= prices.getAmount(TradeGood::Labor)) {
    return 0;
  }
  double highestExtraProduction = expectedProd * extraLabourFactor(extraLabour + maxPossible, expectedLabour) - currentExtraProduction;
  if (highestExtraProduction * prices.getAmount(output) >= maxPossible * prices.getAmount(TradeGood::Labor)) {
    return maxPossible;
  }
  
  double lowestFraction = 0;
  double highestFraction = 1;

  double currentFraction = 0.5 * (highestFraction + lowestFraction);
  for (int i = 0; i < 5; ++i) {
    double candidateProd = expectedProd * extraLabourFactor(extraLabour + currentFraction*maxPossible, expectedLabour) - currentExtraProduction;
    if (candidateProd * prices.getAmount(output) >= currentFraction*maxPossible * prices.getAmount(TradeGood::Labor)) lowestFraction = currentFraction;
    else highestFraction = currentFraction;
    if (highestFraction - lowestFraction < 0.05) {
      return currentFraction*maxPossible;
    }
    currentFraction = 0.5 * (highestFraction + lowestFraction);
  }
  return currentFraction*maxPossible;
}

Forest::Forest ()
  : Building(1, 1, 3)
  , Mirrorable<Forest>()
  , Collective<Forester, ForestStatus, numOwners>()
  , yearsSinceLastTick(0)
{
  createWorkers(this);
}

Forest::Forest (Forest* other)
  : Building(other->marginFactor, other->blockSize, other->workableBlocks)
  , Mirrorable<Forest>(other)
  , Collective<Forester, ForestStatus, numOwners>()
  , yearsSinceLastTick(0)    
{}

Forest::~Forest () {}

Forester::Forester (Forest* b)
  : Industry<Forester>(this, b)
  , Mirrorable<Forester>()
  , fields(ForestStatus::numTypes(), 0)
  , tendedGroves(0)
  , wildForest(0)
{}

Forester::Forester (Forester* other)
  : Industry<Forester>(this, other->blockInfo)
  , Mirrorable<Forester>(other)
  , fields(ForestStatus::numTypes(), 0)
  , tendedGroves(other->tendedGroves)
  , wildForest(other->wildForest)
{}

void Forester::setMirrorState () {
  for (unsigned int i = 0; i < fields.size(); ++i) mirror->fields[i] = fields[i];
  mirror->tendedGroves = tendedGroves;
  mirror->wildForest = wildForest;
  setEconMirrorState(mirror);
}

Forester::~Forester () {}

// NB, this is not intended to take marginal decline into
// account - "block N" is for cases, like Forester, where
// there are different outputs for other reasons than
// margins.
// Also note that this is not the return right now, but the
// expected total possibly years from now, when the trees
// become harvestable.
double Forester::outputOfBlock (int /*block*/) const {
  return (*(ForestStatus::rstart()))->yield * blockInfo->blockSize;
}

void Forest::unitTests () {
  Hex* testHex = Hex::getTestHex(false, false, true, false);
  Forest* testForest = testHex->getForest();
  if ((int) testForest->workers.size() != numOwners) {
    sprintf(errorMessage, "Forest should have %i Workers, has %i", numOwners, testForest->workers.size());
    throw string(errorMessage);
  }

  Forester* testForester = testForest->workers[0];
  testForester->unitTests();
  delete testForest;
  if (testForester->theMarket) throw string("Expected forester to have been unregistered");
}

void Forester::unitTests () {
  // Note that this is not static, it's run on a particular Forester.
  if (!output) throw string("Forest output has not been set.");
  if (0 >= blockInfo->blockSize) throw new string("Expected positive block size");
  GoodsHolder oldCapital(*capital);
  capital->zeroGoods();
  TradeGood const* testCapGood = 0;
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    if ((*tg) == output) continue;
    testCapGood = (*tg);
    capital->setAmount((*tg), 0.1);
    break;
  }

  ForestStatus::Iter clearStatus = ForestStatus::start();
  ForestStatus::Iter plantStatus = ForestStatus::start(); ++plantStatus;
  ForestStatus::rIter hugeStatus = ForestStatus::rstart(); ++hugeStatus;
  ForestStatus::rIter lastStatus = ForestStatus::rstart();
  Calendar::newYearBegins();
  fields[**clearStatus] = 100;
  fields[**hugeStatus] = 100;
  createBlockQueue();
  double fullCycleLabour = 0;
  vector<jobInfo> jobs;
  double laborNeeded = 0;
  getLabourForBlock(0, jobs, laborNeeded);
  if (laborNeeded <= 0) throwFormatted("Expected to need positive amount of labor, but found %f", laborNeeded);

  double laborNeededWithCapital = 0;
  deliverGoods(testCapGood, 1);
  getLabourForBlock(0, jobs, laborNeededWithCapital);
  if (laborNeededWithCapital >= laborNeeded) throwFormatted("With capital %f of %s -> %f, should require less than %f labour, but need %f",
							    getAmount(testCapGood),
							    testCapGood->getName().c_str(),
							    capitalFactor(*this),
							    laborNeeded,
							    laborNeededWithCapital);

  blockInfo->workableBlocks = 200;
  createBlockQueue();
  GoodsHolder prices;
  vector<MarketBid*> bidlist;
  zeroGoods();
  if (0 < getAmount(testCapGood)) throwFormatted("Expected not to have any more capital after clearing; found %f %i", getAmount(testCapGood), testCapGood->getIdx());
  prices.deliverGoods(TradeGood::Labor, 1);
  prices.deliverGoods(testCapGood, 1);
  prices.deliverGoods(output, 100);
  getBids(prices, bidlist);
  
  double foundLabour = 0;
  bool foundCapGood = false;
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) {
      foundLabour += mb->amountToBuy;
    }
    else if (mb->tradeGood == testCapGood) {
      foundCapGood = true;
      if (mb->amountToBuy <= 0) throwFormatted("Expected to buy %s, but am selling", testCapGood->getName().c_str());
    }
    else {
      throwFormatted("Expected to bid on %s and %s, but got bid for %f %s (capital %f price %f) %i %i %f",
		     TradeGood::Labor->getName().c_str(),
		     testCapGood->getName().c_str(),
		     mb->amountToBuy,
		     mb->tradeGood->getName().c_str(),
		     capital->getAmount(mb->tradeGood),
		     prices.getAmount(mb->tradeGood),
		     mb->tradeGood->getIdx(),
		     testCapGood->getIdx(),
		     capital->getAmount(testCapGood));
    }
  }

  double remainingLabour = foundLabour;
  remainingLabour -= fields[**hugeStatus] * (*hugeStatus)->labourToHarvest;
  if (remainingLabour < 0) throwFormatted("Should buy enough %s to harvest %i %s, but only bought %.2f",
					  TradeGood::Labor->getName().c_str(),
					  fields[**hugeStatus],
					  (*hugeStatus)->getName().c_str(),
					  foundLabour);

  int expectedTendedFields = fields[**clearStatus] + fields[**hugeStatus];
  expectedTendedFields /= Calendar::turnsToNextSeason();
  if (expectedTendedFields * Calendar::turnsToNextSeason() < fields[**clearStatus] + fields[**hugeStatus]) ++expectedTendedFields;
  for (int i = 0; i < expectedTendedFields; ++i) {
    if (remainingLabour < 0.001) throwFormatted("Should buy enough %s to tend %i groves, but ran out after %i",
						TradeGood::Labor->getName().c_str(),
						expectedTendedFields,
						i);
    remainingLabour -= (*clearStatus)->labourToTend;
  }
  if (remainingLabour > 0.5) throwFormatted("Forester bought too much labour, total %.2f", foundLabour);
  if (!foundCapGood) throwFormatted("Expected to buy %s, no bid found", testCapGood->getName().c_str());

  owner = this;
  tendedGroves = getTendedArea();
  tendedGroves -= 10;
  extractResources(true);
  if ((fields[**clearStatus] > 0) ||
      (fields[**hugeStatus] > 0) ||
      (fields[**lastStatus] != 95) ||
      (fields[**plantStatus] != 95) ||
      (wildForest != 10))
    throwFormatted("Expected (%s, %s, %s, %s, wild) to be (%i %i %i %i %i), got (%i %i %i %i %i)",
		   (*clearStatus)->getName().c_str(),
		   (*hugeStatus)->getName().c_str(),
		   (*lastStatus)->getName().c_str(),
		   (*plantStatus)->getName().c_str(),
		   0, 0, 95, 95, 10,
		   fields[**clearStatus],
		   fields[**hugeStatus],
		   fields[**lastStatus],
		   fields[**plantStatus],
		   wildForest);

  getLabourForBlock(0, jobs, fullCycleLabour);
  deliverGoods(TradeGood::Labor, fullCycleLabour);
  extractResources(false);
  double firstOutput = getAmount(output);
  if (0.01 > firstOutput) throwFormatted("Expected to have some wood after extractResources %i %i %i %i %i",
					 fields[**clearStatus],
					 fields[**hugeStatus],
					 fields[**lastStatus],
					 fields[**plantStatus],
					 wildForest);

  for (unsigned int i = 0; i < fields.size(); ++i) fields[i] = 0;
  fields[**lastStatus] = blockInfo->blockSize;
  createBlockQueue();
  getLabourForBlock(0, jobs, fullCycleLabour);
  setAmount(TradeGood::Labor, fullCycleLabour);
  setAmount(output, 0);
  tendedGroves = getTendedArea();
  unsigned int beforeWork = myBlocks.size();
  extractResources(false);
  unsigned int afterWork = myBlocks.size();
  if (beforeWork != afterWork) throwFormatted("Change in number of fields during extractResources, %i -> %i", beforeWork, afterWork);
  firstOutput = getAmount(output);

  vector<double> marginFactors;
  marginFactors.push_back(0.99);
  marginFactors.push_back(0.9);
  marginFactors.push_back(0.1);
  BOOST_FOREACH(double mf, marginFactors) {
    blockInfo->marginFactor = mf;
    for (int blocks = 2; blocks < 5; ++blocks) {
      fields[**lastStatus] = blocks*blockInfo->blockSize;
      createBlockQueue();
      double gameExpected = 0;      
      for (int j = 0; j < blocks; ++j) gameExpected += outputOfBlock(j) * pow(mf, j);
      setAmount(output, 0);
      getLabourForBlock(0, jobs, fullCycleLabour);
      setAmount(TradeGood::Labor, blocks*fullCycleLabour);
      tendedGroves = getTendedArea();
      extractResources(false);
      // Geometric sum, with growth rate r, from 0 to n, is (1-r^(n+1)) / (1-r).
      double coderExpected = firstOutput * (1 - pow(mf, blocks)) / (1 - mf);
      double actual = getAmount(output);
      if (0.1 < fabs(coderExpected - actual)) throwFormatted("With margin %f and %i blocks, expected %f but got %f",
							     mf,
							     blocks,
							     coderExpected,
							     actual);
      if (0.1 < fabs(gameExpected - actual)) throwFormatted("With margin %f and %i blocks, heuristic expected %f; actually got %f",
							    mf,
							    blocks,
							    gameExpected,
							    actual);
    }
  }

  capital->setAmounts(oldCapital);
}

void Forester::getLabourForBlock (int block, vector<jobInfo>& jobs, double& prodCycleLabour) const {
  Calendar::Season currSeason = Calendar::getCurrentSeason();
  if (Calendar::Winter == currSeason) return;
  int startIndex = block * blockInfo->blockSize;
  int endIndex   = min(startIndex + blockInfo->blockSize, (int) myBlocks.size());
  prodCycleLabour = 0;
  double capFactor = capitalFactor(*this);
  ForestStatus::rIter hugeStatus = ForestStatus::rstart(); ++hugeStatus;
  for (int i = startIndex; i < endIndex; ++i) {
    if (i >= tendedGroves) {
      searchForMatch(jobs, myBlocks[i]->labourToTend * capFactor, 1, Calendar::turnsToNextSeason());
      prodCycleLabour += myBlocks[i]->labourToTend;
    }
    // TODO: Get from discount rate
    if (*myBlocks[i] >= **hugeStatus) {
      searchForMatch(jobs, myBlocks[i]->labourToHarvest * capFactor, 1, 1);
      prodCycleLabour += myBlocks[i]->labourToHarvest;
    }
  }

  prodCycleLabour *= capFactor;
}

int Forester::numBlocks () const {
  int blocks = myBlocks.size() / blockInfo->blockSize;
  if ((int) myBlocks.size() > blocks * blockInfo->blockSize) ++blocks;
  return min(blocks, blockInfo->workableBlocks);
}

void Forester::extractResources (bool tick) {
  if (tick) {
    ForestStatus::rIter next = ForestStatus::rstart(); ++next;
    ForestStatus::rIter fs = ForestStatus::rstart();
    fields[**fs] += fields[**next];
    for (++fs, ++next; fs != ForestStatus::rfinal(); ++fs, ++next) {
      if (next == ForestStatus::rfinal()) fields[**fs] = 0;
      else fields[**fs] = fields[**next];
    }

    int wildening = getTendedArea() - tendedGroves;
    wildForest += wildening;
    ForestStatus::rIter target = ForestStatus::rstart();
    while (wildening > 0) {
      if (0 < fields[**target]) {
	fields[**target]--;
	wildening--;
      }
      if (++target == ForestStatus::rfinal()) target = ForestStatus::rstart();
    }
    tendedGroves = 0;
    createBlockQueue();
    return;
  }

  double availableLabour = getAmount(TradeGood::Labor);
  double capFactor = capitalFactor(*this);
  double decline = 1;
  double totalChopped = 0;
  ForestStatus::Iter clearStatus = ForestStatus::start();
  ForestStatus::rIter hugeStatus = ForestStatus::rstart(); ++hugeStatus;
  for (int block = 0; block < numBlocks(); ++block) {
    int startIndex = block * blockInfo->blockSize;
    int endIndex   = min(startIndex + blockInfo->blockSize, (int) myBlocks.size());
    for (int i = startIndex; i < endIndex; ++i) {
      if ((tendedGroves <= i) && (availableLabour >= myBlocks[i]->labourToTend * capFactor)) {
	availableLabour -= myBlocks[i]->labourToTend * capFactor;
	++tendedGroves;
      }
      // TODO: Use discount rate
      if ((*myBlocks[i] >= **hugeStatus) && (availableLabour >= myBlocks[i]->labourToHarvest * capFactor)) {
	availableLabour -= myBlocks[i]->labourToHarvest * capFactor;
	totalChopped += myBlocks[i]->yield * decline;
	--fields[*myBlocks[i]];
	++fields[**clearStatus];
      }
    }
    decline *= blockInfo->marginFactor;
  }
  produce(output, totalChopped);
  // This cheats very slightly, by moving fields up in the
  // queue - formerly inefficient ones thus become the best.
  createBlockQueue();

  double usedLabour = availableLabour - getAmount(TradeGood::Labor);
  deliverGoods(TradeGood::Labor, usedLabour);
  if (getAmount(TradeGood::Labor) < 0) throw string("Negative labour after extractResources");
}

double Forester::getCapitalSize () const {
  return myBlocks.size();
}

int Forester::getTendedArea () const {
  int ret = 0;
  for (ForestStatus::Iter fs = ForestStatus::start(); fs != ForestStatus::final(); ++fs) {
    ret += fields[**fs];
  }
  return ret;
}

int Forester::getForestArea () const {
  return getTendedArea() + wildForest;
}

void Forester::createBlockQueue () {
  myBlocks.clear();
  for (ForestStatus::rIter fs = ForestStatus::rstart(); fs != ForestStatus::rfinal(); ++fs) {
    for (int j = 0; j < fields[**fs]; ++j) {
      myBlocks.push_back(*fs);
    }
  }
}

void Forest::setMirrorState () {
  mirror->setOwner(getOwner());
  mirror->workers.clear();
  BOOST_FOREACH(Forester* forester, workers) {
    forester->setMirrorState();
    mirror->workers.push_back(forester->getMirror());
  }
  mirror->yearsSinceLastTick = yearsSinceLastTick;
}

void Village::setMirrorState () {
  women.setMirrorState();
  males.setMirrorState();
  milTrad->setMirrorState();
  mirror->milTrad = milTrad->getMirror();
  mirror->consumptionLevel = consumptionLevel;
  mirror->setOwner(getOwner());
  // Building mirror states set by Hex.
  if (farm) mirror->farm = farm->getMirror();
  setEconMirrorState(mirror);
}

void Farmland::setMirrorState () {
  mirror->setOwner(getOwner());
  mirror->workers.clear();
  BOOST_FOREACH(Farmer* farmer, workers) {
    farmer->setMirrorState();
    mirror->workers.push_back(farmer->getMirror());
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

void Farmland::devastate (int /*devastation*/) {
}

void Farmland::endOfTurn () {
  doWork(false);
  countTotals();
}

void Village::eatFood () {
  double consumptionFactor = consumption();
  consumptionLevel = 0;
  GoodsHolder consumed;
  BOOST_FOREACH(MaslowLevel const* level, maslowLevels) {
    int numGoods = 0;
    double totalAvailable = 1;
    TradeGood::Iter firstProduct = TradeGood::final();
    vector<const TradeGood*> others;
    GoodsHolder effectivePrices;
    double weightedProduct = 1;
    double totalConsumption = consumptionFactor * level->getAmount(TradeGood::Money);
    for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
      double weight = level->getAmount(*tg);
      if (weight < 0.0001) continue;
      ++numGoods;
      totalAvailable *= pow(getAmount(*tg), weight);
      double effPrice = 1.0 / (getAmount(*tg) * weight);
      effectivePrices.setAmount((*tg), effPrice);
      weightedProduct *= pow(effPrice, weight);
      if (firstProduct == TradeGood::final()) firstProduct = tg;
      else others.push_back(*tg);
    }
    if (totalAvailable < totalConsumption) break;
    if (1 == numGoods) {
      EconActor::consume((*firstProduct), totalConsumption);
      consumed.deliverGoods((*firstProduct), totalConsumption);
    }
    else {
      // See substitution section in internaldocs for math.
      others.push_back(*firstProduct);
      GoodsHolder amountToConsume;
      set<const TradeGood*> giveUp;
      double renormFactor = 0;
      while (true) {
	bool goodAssignments = true;
	for (vector<const TradeGood*>::const_iterator tg = others.begin(); tg != others.end(); ++tg) {
	  if (giveUp.count(*tg)) continue;
	  double amount = totalConsumption * weightedProduct;
	  amount /= effectivePrices.getAmount(*tg);
	  if (amount <= getAmount(*tg)) amountToConsume.setAmount((*tg), amount);
	  else {
	    amountToConsume.setAmount((*tg), getAmount(*tg));
	    goodAssignments = false;
	    giveUp.insert(*tg);
	    renormFactor += level->getAmount(*tg);
	    break;
	  }
	}
	if (goodAssignments) break;
	if (giveUp.size() == others.size()) throwFormatted("%s:%i: Unable to find good consumption assignments", __FILE__, __LINE__);
	// Recalculate the weights, effective prices, and weightedProduct without the goods we've given up on.
	for (vector<const TradeGood*>::const_iterator tg = others.begin(); tg != others.end(); ++tg) {
	  if (giveUp.count(*tg)) continue;
	  double weight = level->getAmount(*tg);
	  weight /= (1 - renormFactor);
	  double effPrice = 1.0 / (getAmount(*tg) * weight);
	  effectivePrices.setAmount((*tg), effPrice);
	  weightedProduct *= pow(effPrice, weight);
	}
      }
      for (vector<const TradeGood*>::const_iterator tg = others.begin(); tg != others.end(); ++tg) {
	EconActor::consume((*tg), amountToConsume.getAmount(*tg));
	consumed.deliverGoods((*tg), amountToConsume.getAmount(*tg));
      }
    }

    consumptionLevel = level;
  }

  if ((isReal()) && (getGraphicsInfo()) && (consumptionLevel)) {
    getGraphicsInfo()->addEvent(DisplayEvent(createString("Consumption level %s", consumptionLevel->name.c_str()),
					     createString("Goods used:%s", consumed.display().c_str())));
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
  doWork(tick);
}

double Farmland::expectedProduction () const {
  double ret = 0;
  BOOST_FOREACH(Farmer* farmer, workers) {
    double margin = 1;
    for (int i = 0; i < farmer->numBlocks(); ++i) {
      ret += farmer->outputOfBlock(i) * margin;
      margin *= marginFactor;
    }
  }
  return ret;
}

double Farmland::possibleProductionThisTurn () const {
  if (Calendar::Autumn != Calendar::getCurrentSeason()) return 0;
  double ret = 0;
  BOOST_FOREACH(Farmer* farmer, workers) {
    for (FieldStatus::Iter fs = FieldStatus::startReap(); fs != FieldStatus::finalReap(); ++fs) {
      ret += farmer->fields[**fs] * (*fs)->yield;
    }
  }
  return ret;
}

void Farmland::countTotals () {
  for (FieldStatus::Iter fs = FieldStatus::start(); fs != FieldStatus::final(); ++fs) {
    totalFields[**fs] = 0;
    BOOST_FOREACH(Farmer* farmer, workers) {
      totalFields[**fs] += farmer->fields[**fs];
    }
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
  : Building(1, 1, 1)
  , Mirrorable<Mine>()
  , Collective<Miner, MineStatus, 10>()
{
  createWorkers(this);
}

Mine::Mine (Mine* other)
  : Building(other->marginFactor, other->blockSize, other->workableBlocks)
  , Mirrorable<Mine>(other)
  , Collective<Miner, MineStatus, 10>()
{}

Mine::~Mine () {}

Miner::Miner (Mine* m)
  : Industry<Miner>(this, m)
  , Mirrorable<Miner>()
  , fields(MineStatus::numTypes(), 0)
{}

Miner::Miner (Miner* other)
  : Industry<Miner>(this, other->blockInfo)
  , Mirrorable<Miner>(other)
  , fields(MineStatus::numTypes(), 0)
{}

int Miner::numBlocks () const {
  return blockInfo->workableBlocks;
}

void Miner::setMirrorState () {
  for (unsigned int i = 0; i < fields.size(); ++i) mirror->fields[i] = fields[i];
  setEconMirrorState(mirror);
}

Miner::~Miner () {}

MineStatus::MineStatus (string n, int rl, int wl)
  : Enumerable<MineStatus>(this, n, false)
  , requiredLabour(rl)
  , winterLabour(wl)
{}

MineStatus::~MineStatus () {}

FieldStatus::FieldStatus (string n, int sprl, int suml, int autl, int winl, int y)
  : Enumerable<const FieldStatus>(this, n, false)
  , springLabour(sprl)
  , summerLabour(suml)
  , autumnLabour(autl)
  , winterLabour(winl)
  , yield(y)
{}

FieldStatus::~FieldStatus () {}

void FieldStatus::clear () {
  Enumerable<const FieldStatus>::clear();
  harvest.clear();
  weeding.clear();
  plowing.clear();
}

ForestStatus::ForestStatus (string n, int y, int tl, int hl)
  : Enumerable<const ForestStatus>(this, n, false)
  , yield(y)
  , labourToTend(tl)
  , labourToHarvest(hl)
{}

ForestStatus::~ForestStatus () {}

void Mine::endOfTurn () {
  doWork();
}

double Miner::getCapitalSize () const {
  return numBlocks();
}

double Miner::getWinterLabour (const GoodsHolder& /*prices*/, int lastBlock, double /*expectedProd*/, double /*expectedLabour*/) const {
  for (MineStatus::Iter ms = MineStatus::start(); ms != MineStatus::final(); ++ms) {
    if (0 == fields[**ms]) continue;
    return (*ms)->winterLabour * lastBlock;
  }
  return 0;
}

double Miner::outputOfBlock (int /*block*/) const {
  return Mine::_amountOfIron;
}

void Mine::unitTests () {
  if (3 > MineStatus::numTypes()) {
    sprintf(errorMessage, "Expected at least 3 MineStatus entries, got %i", MineStatus::numTypes());
    throw string(errorMessage);
  }
  Hex* testHex = Hex::getTestHex(false, false, false, true);
  Mine* testMine = testHex->getMine();
  if ((int) testMine->workers.size() != numOwners) throwFormatted("Mine should have %i Workers, has %i", numOwners, testMine->workers.size());
  Miner* testMiner = testMine->workers[0];
  testMiner->unitTests();
  delete testMine;
  if (testMiner->theMarket) throw string("Expected miner to have been unregistered");
}

void Miner::unitTests () {
  if (!output) throw string("Mine output has not been set.");
  if (!theMarket) throw string("No market registered");
  // Note that this is not static, it's run on a particular Miner.
  GoodsHolder oldCapital(*capital);
  TradeGood const* testCapGood = 0;
  capital->zeroGoods();
  for (TradeGood::Iter tg = TradeGood::exLaborStart(); tg != TradeGood::final(); ++tg) {
    testCapGood = (*tg);
    capital->setAmount((*tg), 0.1);
    break;
  }

  MineStatus::Iter msi = MineStatus::start();
  MineStatus* status = (*msi);
  fields[*status] = 100;
  double fullCycleLabour = 0;
  vector<jobInfo> jobs;
  double laborNeeded = 0;
  getLabourForBlock(0, jobs, laborNeeded);
  if (laborNeeded <= 0) throwFormatted("Expected to need positive amount of labor, but found %f", laborNeeded);

  deliverGoods(testCapGood, 1);
  double laborNeededWithCapital = 0;
  getLabourForBlock(0, jobs, laborNeededWithCapital);
  if (laborNeededWithCapital >= laborNeeded) throwFormatted("With capital %f of %s, should require less than %f labour, but need %f",
							    getAmount(testCapGood),
							    testCapGood->getName().c_str(),
							    laborNeeded,
							    laborNeededWithCapital);
  deliverGoods(testCapGood, -1);

  getLabourForBlock(0, jobs, laborNeeded);
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

  if (fabs(foundLabour - status->requiredLabour - status->winterLabour) > 0.1) throwFormatted("Expected to buy %i %s, bid is for %f",
											      status->requiredLabour + status->winterLabour,
											      TradeGood::Labor->getName().c_str(),
											      foundLabour);

  getLabourForBlock(0, jobs, fullCycleLabour);
  if (0.1 > foundLabour) throwFormatted("Expected to buy %s, no bid found (%f %f)",
					TradeGood::Labor->getName().c_str(),
					outputOfBlock(0),
					fullCycleLabour);
  if (!foundCapGood) throwFormatted("Expected to buy %s, no bid found", testCapGood->getName().c_str());

  owner = this;
  deliverGoods(TradeGood::Labor, laborNeeded);
  extractResources();
  if (fields[*status] != 99) throwFormatted("Expected %s to be 99, got %i", status->getName().c_str(), fields[*status]);

  double productionOne = getAmount(output);
  if (0.01 > productionOne) throw string("Expected to have some iron after extractResources");

  // Check that, with marginal production of zero, we get the same labor bid.
  // That is, second block produces nothing, so should not create a bid.
  blockInfo->workableBlocks = 2;
  blockInfo->marginFactor = 0;
  bidlist.clear();
  getBids(prices, bidlist);
  double newFoundLabour = 0;
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) newFoundLabour += mb->amountToBuy;
  }
  if (fabs(foundLabour - newFoundLabour) > 0.1) throwFormatted("With zero margin, expected to buy %f, but got %f",
							       foundLabour,
							       newFoundLabour);

  // With no marginal decline, should buy twice as much.
  blockInfo->marginFactor = 1;
  bidlist.clear();
  getBids(prices, bidlist);
  newFoundLabour = 0;
  BOOST_FOREACH(MarketBid* mb, bidlist) {
    if (mb->tradeGood == TradeGood::Labor) newFoundLabour += mb->amountToBuy;
  }
  if (fabs(blockInfo->workableBlocks*foundLabour - newFoundLabour) > 0.1) throwFormatted("With margin 1, expected to buy %f, but got %f",
										   blockInfo->workableBlocks*foundLabour,
										   newFoundLabour);

  setAmount(output, 0);
  getLabourForBlock(0, jobs, laborNeeded);
  setAmount(TradeGood::Labor, numBlocks() * laborNeeded);
  extractResources();
  double productionTwo = getAmount(output);
  if (0.1 < fabs(productionTwo - numBlocks() * productionOne)) {
    sprintf(errorMessage, "With %i blocks and margin factor 1, expected to produce %f, but got %f", numBlocks(), numBlocks() * productionOne, productionTwo);
    throw string(errorMessage);
  }

  blockInfo->marginFactor = 0.5;
  setAmount(output, 0);
  getLabourForBlock(0, jobs, laborNeeded);
  setAmount(TradeGood::Labor, numBlocks() * laborNeeded);
  extractResources();
  double productionThree = getAmount(output);
  if (0.1 < fabs(productionThree - 1.5 * productionOne)) {
    sprintf(errorMessage, "With two blocks and margin factor 0.5, expected to produce %f, but got %f", 1.5 * productionOne, productionThree);
    throw string(errorMessage);
  }

  blockInfo->marginFactor = 0.75;
  blockInfo->workableBlocks = 3;
  setAmount(output, 0);
  getLabourForBlock(0, jobs, laborNeeded);
  setAmount(TradeGood::Labor, numBlocks() * laborNeeded);
  extractResources();
  double productionFour = getAmount(output);
  if (0.1 < fabs(productionFour - 2.3125 * productionOne)) throwFormatted("With three blocks and margin factor 0.75, expected to produce %f, but got %f",
									  2.3125 * productionOne,
									  productionThree);
  capital->setAmounts(oldCapital);
}

void Miner::getLabourForBlock (int /*block*/, vector<jobInfo>& jobs, double& prodCycleLabour) const {
  double ret = 0;
  for (MineStatus::Iter ms = MineStatus::start(); ms != MineStatus::final(); ++ms) {
    if (0 == fields[**ms]) continue;
    ret = (*ms)->requiredLabour;
    break;
  }

  ret *= capitalFactor(*this);
  prodCycleLabour = ret;
  searchForMatch(jobs, ret, 1, 1);
}

void Miner::extractResources (bool /*tick*/) {
  double availableLabour = getAmount(TradeGood::Labor);
  double capFactor = capitalFactor(*this);

  double decline = 1;
  double mined = 0;
  for (MineStatus::Iter ms = MineStatus::start(); ms != MineStatus::final(); ++ms) {
    if (0 == fields[**ms]) continue;
    for (int i = 0; i < numBlocks(); ++i) {
      if (availableLabour < (*ms)->requiredLabour * capFactor) break;
      availableLabour -= (*ms)->requiredLabour * capFactor;
      mined += Mine::_amountOfIron * decline;
      if (0 == --fields[**ms]) break;
      decline *= blockInfo->marginFactor;
    }

    break;
  }

  mined *= extraLabourFactor(availableLabour, getAmount(TradeGood::Labor) - availableLabour);
  produce(output, mined);
  setAmount(TradeGood::Labor, 0);
}

void Mine::setMirrorState () {
  mirror->setOwner(getOwner());
  mirror->workers.clear();
  BOOST_FOREACH(Miner* miner, workers) {
    miner->setMirrorState();
    mirror->workers.push_back(miner->getMirror());
  }
}

void searchForMatch (vector<jobInfo>& jobs, double perChunk, int chunks, int time) {
  searchForMatch(jobs, jobInfo(perChunk, chunks, time));
}

void searchForMatch (vector<jobInfo>& jobs, jobInfo newjob) {
  double perChunk = newjob.labourPerChunk();
  if (0 >= perChunk) return;
  int chunks = newjob.numChunks();
  if (0 >= chunks) return;
  int time = newjob.numTurns();
  BOOST_FOREACH(jobInfo& job, jobs) {
    if (fabs(job.labourPerChunk() - perChunk) > 0.001) continue;
    if (job.numTurns() != time) continue;
    job.numChunks() += chunks;
    return;
  }
  jobs.push_back(newjob);
}
