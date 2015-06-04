#include "Market.hh"
#include "boost/range/algorithm/remove_if.hpp"
#include "boost/bind.hpp"

void MarketContract::clear () {
  cashPaid = 0;
  delivered = 0;
}

double MarketContract::deliver (double amountWanted) {
  double amountAvailable = producer->produceForContract(tradeGood, amountWanted);
  delivered += amountAvailable;
  recipient->deliverGoods(tradeGood, amountAvailable);
  if (recipient->getAmount(tradeGood) < 0) throwFormatted("Negative %s (%f) after delivery", tradeGood->getName().c_str(), recipient->getAmount(tradeGood));
  return amountAvailable;
}

double MarketContract::execute () {
  double amountWanted = remaining();
  if (amountWanted < 0.001) return 0;
  double moneyAvailable = min(recipient->getAmount(TradeGood::Money), amountWanted * price);
  recipient->deliverGoods(TradeGood::Money, -moneyAvailable);
  producer->deliverGoods(TradeGood::Money, moneyAvailable);
  cashPaid += moneyAvailable;
  if (moneyAvailable < amountWanted * price) moneyAvailable += producer->extendCredit(recipient, (amountWanted*price - moneyAvailable));
  if (moneyAvailable < amountWanted * price) amountWanted = moneyAvailable / price;
  return deliver(amountWanted);
}

doublet MarketContract::execute (MarketContract* other) {
  // A pair of contracts that amount to barter, eg labour for food.
  // Executing these directly, without going through money, is an
  // optimisation; it avoids having a large amount of small deliveries
  // if the parties have little money and credit.

  if (recipient != other->producer) return doublet(0, 0);
  if (producer != other->recipient) return doublet(0, 0);

  double amountWanted = remaining();
  if (amountWanted < 0.001) return doublet(0, 0);

  double otherWanted = other->remaining();
  if (otherWanted < 0.001) return doublet(0, 0);

  double moneyToExchange = min(amountWanted * price, otherWanted * price);
  cashPaid += moneyToExchange;
  other->cashPaid += moneyToExchange;
  return doublet(deliver(moneyToExchange / price), other->deliver(moneyToExchange / other->price));
}

void MarketContract::pay () {
  double moneyOwed = max(delivered * price - cashPaid, 0.0);
  double moneyAvailable = min(recipient->getAmount(TradeGood::Money), moneyOwed);
  producer->getPaid(recipient, moneyAvailable);
  accumulatedMissing += (amount - delivered);
  --remainingTime;
}

Market::Market ()
  : Mirrorable<Market>()
{
  // Initialise all prices to 1.
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    prices.setAmount((*tg), 1);
  }
}

Market::~Market () {
  // Just in case some complete idiot hasn't deleted them already, so they don't try to unregister.
  participants.clear(); 
}

Market::Market (Market* other)
  : Mirrorable<Market>(other)
{
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    prices.setAmount((*tg), 1);
  }
}

void Market::executeContracts () {
  volume.zeroGoods();
  BOOST_FOREACH(MarketContract* mc, contracts) mc->clear();
  for (vector<MarketContract*>::iterator c1 = contracts.begin(); c1 != contracts.end(); ++c1) {
    for (vector<MarketContract*>::iterator c2 = c1 + 1; c2 != contracts.end(); ++c2) {
      if (((*c1)->recipient == (*c2)->producer) && ((*c2)->recipient == (*c1)->producer)) {
	doublet traded = (*c1)->execute(*c2);
	volume.deliverGoods((*c1)->tradeGood, traded.x());
	volume.deliverGoods((*c2)->tradeGood, traded.y());
      }
    }
  }

  int counter = 0;
  double traded = 0;
  for (counter = 0; counter < 10000; ++counter) {
    traded = 0;
    BOOST_FOREACH(MarketContract* mc, contracts) {
      double currTrade = mc->execute();
      traded += currTrade;
      volume.deliverGoods(mc->tradeGood, currTrade);
    }
    if (0.001 > traded) break;
  }
  BOOST_FOREACH(MarketContract* mc, contracts) mc->pay();
}

void Market::holdMarket () {
  consumed.zeroGoods();
  produced.zeroGoods();
  vector<MarketBid*> bidlist;
  vector<MarketBid*> notMatched;
  BOOST_FOREACH(EconActor* ea, participants) ea->getBids(prices, bidlist);
  makeContracts(bidlist, notMatched);
  executeContracts();
  adjustPrices(notMatched);
  normalisePrices();
  BOOST_FOREACH(MarketBid* mb, notMatched) delete mb;
  BOOST_FOREACH(EconActor* ea, participants) ea->dunAndPay();
  vector<MarketContract*>::iterator new_end = remove_if(contracts, !bind(&MarketContract::isValid, _1));
  for (vector<MarketContract*>::iterator i = new_end; i != contracts.end(); ++i) delete (*i);
  contracts.erase(new_end, contracts.end());
}

void Market::makeContracts (vector<MarketBid*>& bids, vector<MarketBid*>& notMatched) {
  // Match buyers and sellers to create Contracts.
  while (bids.size()) {
    MarketBid* toMatch = bids.back();
    if (!toMatch) throw string("Popped null bid");
    if (!toMatch->bidder) throw string ("Bid with null bidder");
    if (!toMatch->tradeGood) throw string ("Bid with no trade good");
    bids.pop_back();
    MarketBid* match = 0;
    BOOST_FOREACH(MarketBid* mb, bids) {
      // Search for a bid with the same good, but opposite sign.
      if (mb->tradeGood != toMatch->tradeGood) continue;
      if (mb->amountToBuy * toMatch->amountToBuy > 0) continue;      
      if (mb->bidder == toMatch->bidder) throwFormatted("Bidder %i trying to buy and sell %s at the same time", mb->bidder->getIdx(), mb->tradeGood->getName().c_str());
      if (mb == toMatch) throw string("Bid appears twice in bid list");
      match = mb;
      break;
    }
    if (!match) {
      notMatched.push_back(toMatch);
      continue;
    }
    REMOVE(bids, match);
    if (fabs(toMatch->amountToBuy) < fabs(match->amountToBuy)) {
      MarketBid* temp = toMatch;
      toMatch = match;
      match = temp;
    }

    MarketContract* contract = new MarketContract(toMatch, match, prices.getAmount(toMatch->tradeGood), min(toMatch->duration, match->duration));
    contracts.push_back(contract);
    toMatch->amountToBuy += match->amountToBuy;
    delete match;
    if (fabs(toMatch->amountToBuy) < 0.1) delete toMatch;
    else bids.push_back(toMatch);
  }
}

void Market::normalisePrices () {
  double normFactor = 10.0 / prices.getAmount(TradeGood::Labor);
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    prices.setAmount((*tg), prices.getAmount(*tg) * normFactor);
  }
}

void Market::adjustPrices (vector<MarketBid*>& notMatched) {
  // Count up leftover bids in each good and adjust price
  // upwards if there are leftover buyers, downwards if there
  // are leftover sellers.

  demand.zeroGoods();
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    BOOST_FOREACH(MarketBid* mb, notMatched) {
      if (mb->tradeGood != (*tg)) continue;
      demand.deliverGoods((*tg), mb->amountToBuy);
    }
    double currentVolume = volume.getAmount(*tg);
    if (currentVolume < 1) currentVolume = 1;
    double ratio = fabs(demand.getAmount(*tg));
    if (ratio * 10000 < currentVolume) continue;
    ratio /= currentVolume;

    // Want price response of 1% when unfilled orders are 10% of volume,
    // otherwise linear, but never larger than 25%.
    ratio *= 0.1;
    if (ratio > 0.25) ratio = 0.25;
    if (demand.getAmount(*tg) < 0) ratio *= ((*tg)->getStickiness() - 1); // More sellers than buyers, reduce price. Prices are sticky downwards!
    prices.deliverGoods((*tg), ratio * prices.getAmount(*tg));
  } 
}

void Market::unitTests () {
  Market testMarket;
  EconActor buyer;
  EconActor seller;

  vector<MarketBid*> bids;
  vector<MarketBid*> notMatched;

  // Exact match of buy and sell offers should result in one contract.
  bids.push_back(new MarketBid(TradeGood::Labor, 100, &buyer, 1));
  bids.push_back(new MarketBid(TradeGood::Labor, -100, &seller, 1));
  testMarket.prices.deliverGoods(TradeGood::Labor, 1);
  testMarket.makeContracts(bids, notMatched);
  if (0 != notMatched.size()) throw string("Bids should match");
  if (0 != bids.size()) throw string("No bids should be left over");
  if (1 != testMarket.contracts.size()) throw string("Exactly one contract should be created");
  MarketContract* contract = testMarket.contracts.back();
  if (1 != contract->remainingTime) throwFormatted("Expected contract duration to be 1, found %i", contract->remainingTime);
  testMarket.contracts.clear();
  if (fabs(contract->amount - 100) > 0.001) throw string("Fix this error string");
  if (contract->tradeGood != TradeGood::Labor) throw string("Fix this error string");
  if (contract->producer != &seller) throw string("Fix this error string");
  if (contract->recipient != &buyer) throw string("Fix this error string");
  if (contract->delivered != 0) throw string("Fix this error string");
  if (fabs(contract->price - testMarket.prices.getAmount(contract->tradeGood)) > 0.001) throw string("Fix this error string");
  contract->delivered = 100;
  buyer.deliverGoods(TradeGood::Money, 1000);
  contract->pay();
  if (fabs(seller.getAmount(TradeGood::Money) - contract->delivered * contract->price) > 0.01) throw string("Fix this error string");

  // Buy offer only should not give a new contract, but should give a no-match.
  bids.push_back(new MarketBid(TradeGood::Labor, 100, &buyer, 1));
  testMarket.makeContracts(bids, notMatched);
  if (0 != testMarket.contracts.size()) throw string("Fix this error string");
  if (1 != notMatched.size()) throw string("Fix this error string");
  if (0 != bids.size()) throw string("Fix this error string");
 
  // Unmatched buy offer should result in increased price.
  double oldLabourPrice = testMarket.prices.getAmount(TradeGood::Labor);
  testMarket.adjustPrices(notMatched);
  double newLabourPrice = testMarket.prices.getAmount(TradeGood::Labor);
  if (oldLabourPrice >= newLabourPrice) throw string("Fix this error string");

  // Conversely, sell offer should reduce price.
  oldLabourPrice = newLabourPrice;
  notMatched.clear();
  notMatched.push_back(new MarketBid(TradeGood::Labor, -100, &seller, 1));
  testMarket.adjustPrices(notMatched);
  newLabourPrice = testMarket.prices.getAmount(TradeGood::Labor);
  if (oldLabourPrice <= newLabourPrice) throw string("Fix this error string");

  notMatched.clear();
  oldLabourPrice = newLabourPrice;
  EconActor* secondBuyer = new EconActor();
  bids.push_back(new MarketBid(TradeGood::Labor, -100, &seller, 1));
  bids.push_back(new MarketBid(TradeGood::Labor, 90, secondBuyer, 1));
  bids.push_back(new MarketBid(TradeGood::Labor, 25, &buyer, 1));
  testMarket.makeContracts(bids, notMatched);
  // Should make two contracts, with some buy left over.
  if (2 != testMarket.contracts.size()) throw string("Fix this error string");
  if (1 != notMatched.size()) throw string("Fix this error string");
  testMarket.adjustPrices(notMatched);
  // Leftover buy should increase price.
  newLabourPrice = testMarket.prices.getAmount(TradeGood::Labor);
  if (oldLabourPrice >= newLabourPrice) throw string("Fix this error string");

  // Check stability of prices with ideal buyer and seller.
  testMarket.participants.clear();
  testMarket.contracts.clear();
  testMarket.volume.zeroGoods();
  testMarket.prices.zeroGoods();
  testMarket.demand.zeroGoods();

  class Labourer : public EconActor {
  public:
    Labourer (TradeGood const* const tg) : EconActor(), food(tg) {setAmount(TradeGood::Money, 1e7);}
    virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {
      double labourToSell = prices.getAmount(TradeGood::Labor);
      bidlist.push_back(new MarketBid(TradeGood::Labor, -labourToSell, this, 1));
      double expectedWages = labourToSell * prices.getAmount(TradeGood::Labor);
      double foodToBuy = expectedWages / prices.getAmount(food);
      bidlist.push_back(new MarketBid(food, foodToBuy, this, 1));
    }
    virtual double produceForContract (TradeGood const* const tg, double amount) {if (tg == TradeGood::Labor) return amount; return 0;}
  private:
    TradeGood const* food;
  };
  class FoodProducer : public EconActor {
  public:
    FoodProducer (TradeGood const* const tg) : EconActor(), output(tg) {setAmount(TradeGood::Money, 1e7);}
    virtual void getBids (const GoodsHolder& prices, vector<MarketBid*>& bidlist) {
      double labourToBuy = 20 - prices.getAmount(TradeGood::Labor);
      bidlist.push_back(new MarketBid(TradeGood::Labor, labourToBuy, this, 1));
      double foodToSell = labourToBuy;
      bidlist.push_back(new MarketBid(output, -foodToSell, this, 1));
    }
    virtual double produceForContract (TradeGood const* const tg, double amount) {if (tg == output) return amount; return 0;}
  private:
    TradeGood const* output;
  };

  TradeGood const* food = *(TradeGood::exLaborStart());
  Labourer labourer(food);
  FoodProducer foodProducer(food);
  testMarket.registerParticipant(&labourer);
  testMarket.registerParticipant(&foodProducer);

  vector<pair<double, double> > initialPricePairs;
  initialPricePairs.push_back(pair<double, double>(10, 10));
  initialPricePairs.push_back(pair<double, double>(10, 15));
  initialPricePairs.push_back(pair<double, double>(10, 40));
  initialPricePairs.push_back(pair<double, double>(10, 90));
  initialPricePairs.push_back(pair<double, double>(10, 8));
  initialPricePairs.push_back(pair<double, double>(10, 1));
  for (vector<pair<double, double> >::iterator currPrices = initialPricePairs.begin(); currPrices != initialPricePairs.end(); ++currPrices) {
    testMarket.prices.setAmount(TradeGood::Labor, (*currPrices).first);
    testMarket.prices.setAmount(food, (*currPrices).second);

    for (int i = 0; i < 25; ++i) {
      double oldLabourPrice = testMarket.prices.getAmount(TradeGood::Labor);
      double oldFoodPrice = testMarket.prices.getAmount(food);
      testMarket.volume.zeroGoods();
      testMarket.demand.zeroGoods();
      testMarket.holdMarket();
      if (0 < testMarket.contracts.size()) throwFormatted("Contracts of duration 1 should have been removed, found %i", testMarket.contracts.size());
      if (testMarket.volume.getAmount(food) < 0.01) throwFormatted("With prices (%f, %f), iteration %i had tiny volume %f for %s",
								   (*currPrices).first,
								   (*currPrices).second,
								   i,
								   testMarket.volume.getAmount(food),
								   food->getName().c_str());
      if (testMarket.volume.getAmount(TradeGood::Labor) < 0.01) throwFormatted("With prices (%f, %f), iteration %i had tiny volume %f for labour",
									       (*currPrices).first,
									       (*currPrices).second,
									       i,
									       testMarket.volume.getAmount(TradeGood::Labor));
      double newLabourPrice = testMarket.prices.getAmount(TradeGood::Labor);
      double newFoodPrice = testMarket.prices.getAmount(food);
      double oldEqDistance = fabs(oldLabourPrice - 10)/oldLabourPrice + fabs(oldFoodPrice - 10)/oldFoodPrice;
      double newEqDistance = fabs(newLabourPrice - 10)/newLabourPrice + fabs(newFoodPrice - 10)/newFoodPrice;
      // Allow a small amount of slop because our production
      // functions are really weird.
      if (oldEqDistance < 0.85*newEqDistance) throwFormatted("Prices from (%f, %f) %i moved away from equilibrium: Labour %f -> %f, food %f -> %f, %f %f",
							     (*currPrices).first,
							     (*currPrices).second,
							     i,
							     oldLabourPrice,
							     newLabourPrice,
							     oldFoodPrice,
							     newFoodPrice,
							     testMarket.volume.getAmount(TradeGood::Labor),
							     testMarket.volume.getAmount(food));
    }
  }
}

void Market::registerParticipant (EconActor* ea) {
  if (find(participants.begin(), participants.end(), ea) != participants.end()) return;
  participants.push_back(ea);
  ea->theMarket = this;
}

void Market::setMirrorState () {
  mirror->prices.setAmounts(prices);
}

void Market::unRegisterParticipant (EconActor* ea) {
  vector<EconActor*>::iterator p = find(participants.begin(), participants.end(), ea);
  if (p == participants.end()) return;
  participants.erase(p);
  ea->theMarket = 0;
}

MarketContract::MarketContract (EconActor* s, EconActor* r, double p, unsigned int rmt, const TradeGood* tg, double amt)
  : recipient(r)
  , producer(s)
  , tradeGood(tg)
  , amount(amt)
  , cashPaid(0)
  , delivered(0)
  , price(p)
  , remainingTime(rmt)
  , accumulatedMissing(0)
{
  producer->registerContract(this);
  if (recipient) recipient->registerContract(this);
}

MarketContract::MarketContract (MarketBid* one, MarketBid* two, double p, unsigned int rmt) 
  : recipient(one->bidder)
  , producer(two->bidder)
  , tradeGood(one->tradeGood)
  , amount(min(fabs(one->amountToBuy), fabs(two->amountToBuy)))
  , cashPaid(0)
  , delivered(0)
  , price(p)
  , remainingTime(rmt)
  , accumulatedMissing(0)
{
  if (one->amountToBuy < 0) {
    // One is selling, two is buying.
    recipient = two->bidder;
    producer = one->bidder;
  }
  recipient->registerContract(this);
  producer->registerContract(this);
}

MarketContract::~MarketContract () {
  recipient->unregisterContract(this);
  producer->unregisterContract(this);
}
