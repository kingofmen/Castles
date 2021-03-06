#include "EconActor.hh"
#include "Calendar.hh"
#include "Market.hh"
#include "StructUtils.hh"
#include "Logger.hh" 
#include <cassert>
#include <algorithm> 

TradeGood const* TradeGood::Money = 0;
TradeGood const* TradeGood::Labor = 0;

GoodsHolder::GoodsHolder ()
  : tradeGoods(TradeGood::numTypes(), 0)
{}

GoodsHolder::GoodsHolder (const GoodsHolder& other)
  : tradeGoods(TradeGood::numTypes(), 0)
{
  setAmounts(other);
}

void GoodsHolder::deliverGoods (const GoodsHolder& gh) {
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] += gh.getAmount(*tg);
  }
}

string GoodsHolder::display (int indent) const {
  string ret;
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    if (1 > getAmount(*tg)) continue;
    ret += createString("\n%*.s%.2f ", indent, "", getAmount(*tg)) + (*tg)->getName();
  }
  return ret;
}

GoodsHolder GoodsHolder::loot (double lootRatio) {
  GoodsHolder ret = (*this) * lootRatio;
  (*this) -= ret;
  return ret;
}

void GoodsHolder::setAmounts (GoodsHolder const* const gh) {
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] = gh->getAmount(*tg);
  }
}

void GoodsHolder::setAmounts (const GoodsHolder& gh) {
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] = gh.getAmount(*tg);
  }
}

void GoodsHolder::zeroGoods () {
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] = 0;
  }
}

void GoodsHolder::operator+= (const GoodsHolder& other) {
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] += other.tradeGoods[**tg];
  }
}

void GoodsHolder::operator-= (const GoodsHolder& other) {
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] -= other.tradeGoods[**tg];
  }
}

void GoodsHolder::operator*= (const double scale) {
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] *= scale;
  }
}

GoodsHolder operator* (const GoodsHolder& gh, const double scale) {
  GoodsHolder ret(gh);
  ret *= scale;
  return ret;
}

GoodsHolder operator* (const double scale, const GoodsHolder& gh) {
  GoodsHolder ret(gh);
  ret *= scale;
  return ret;
}

double operator* (const GoodsHolder& gh1, const GoodsHolder& gh2) {
  double ret = 0;
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    ret += gh1.getAmount(*tg) * gh2.getAmount(*tg);
  }
  return ret;
}

EconActor::EconActor ()
  : Numbered<EconActor>()
  , GoodsHolder()
  , TextBridge()
  , owner(0)
  , theMarket(0)
  , obligations()
  , borrowers()
  , discountRate(0.10)
{}

EconActor::~EconActor () {
  leaveMarket();
}

void EconActor::consume (TradeGood const* const tg, double amount) {
  double amountActuallyUsed = amount * tg->getConsumption();
  deliverGoods(tg, -amountActuallyUsed);
  if (theMarket) theMarket->registerConsumption(tg, amountActuallyUsed);
}

void EconActor::setEconMirrorState (EconActor* ea) {
  econMirror = ea;
  econMirror->setAmounts(*this);
  econMirror->setEconOwner(owner ? owner->getEconMirror() : 0);
}

void EconActor::produce (TradeGood const* const tg, double amount) {
  deliverGoods(tg, amount);
  theMarket->registerProduction(tg, amount);
}

void ContractInfo::execute () const {
  if (!recipient) return;
  if (!source) return;

  double delivered = source->produceForTaxes(tradeGood, amount, delivery);
  recipient->receiveTaxes(tradeGood, delivered);
}

double EconActor::availableCredit (EconActor* const applicant) const {
  if ((isOwnedBy(applicant)) || (applicant->isOwnedBy(this))) return 1e7;
  static const double maxCredit = 100;
  double borrowed = 0;
  if (borrowers.count(applicant)) borrowed = borrowers.find(applicant)->second; // Use find for const-ness.
  double amountAvailable = maxCredit - borrowed;
  if (amountAvailable < 0) return 0;
  return amountAvailable;
}

void EconActor::clearRecord () {
  soldThisTurn.zeroGoods();
  earnedThisTurn.zeroGoods();
}

void EconActor::dunAndPay () {
  for (map<EconActor* const, double>::iterator borrower = borrowers.begin(); borrower != borrowers.end(); ++borrower) {
    EconActor* const victim = (*borrower).first;
    double amountToPay = min((*borrower).second, 0.5*victim->getAmount(TradeGood::Money));
    getPaid(victim, amountToPay);
  }

  if ((Calendar::Winter == Calendar::getCurrentSeason()) && (owner)) {
    double amountToPay = 0.5 * getAmount(TradeGood::Money);
    owner->deliverGoods(TradeGood::Money, amountToPay);
    deliverGoods(TradeGood::Money, -amountToPay);
  }
}

double EconActor::extendCredit (EconActor* const applicant, double amountWanted) {
  double amountAvailable = min(availableCredit(applicant), amountWanted);
  if (amountAvailable <= 0) return 0;
  borrowers[applicant] += amountAvailable;
  return amountAvailable;
}

void EconActor::getPaid (EconActor* const payer, double amount) {
  deliverGoods(TradeGood::Money, amount);
  payer->deliverGoods(TradeGood::Money, -amount);
  if (borrowers[payer] > 0) {
    borrowers[payer] -= amount;
    if (borrowers[payer] < 0) borrowers[payer] = 0;
  }
}

void EconActor::leaveMarket () {
  if (theMarket) theMarket->unRegisterParticipant(this);
  theMarket = 0;
}

void EconActor::registerSale (TradeGood const* const tg, double amount, double price) {
  soldThisTurn.deliverGoods(tg, amount);
  earnedThisTurn.deliverGoods(TradeGood::Money, amount*price);
}

double EconActor::produceForContract (TradeGood const* const tg, double amount) {
  amount = min(amount, getAmount(tg));
  deliverGoods(tg, -amount);
  return amount;
}

double EconActor::produceForTaxes (TradeGood const* const tg, double amount, ContractInfo::AmountType taxType) {
  if (ContractInfo::Percentage == taxType) amount *= getAmount(tg);
  amount = min(amount, getAmount(tg));
  deliverGoods(tg, -amount);
  return amount;
}

void EconActor::registerContract (MarketContract const* const contract) {
  double sign = 0;
  if (contract->recipient == this) sign = -1;
  else if (contract->producer == this) sign = 1;
  promisedToDeliver.deliverGoods(contract->tradeGood, sign*contract->amount);
  promisedToDeliver.deliverGoods(TradeGood::Money, -sign * contract->amount * contract->price);
}

void EconActor::unregisterContract (MarketContract const* const contract) {
  double sign = 0;
  if (contract->recipient == this) sign = 1;
  else if (contract->producer == this) sign = -1;
  promisedToDeliver.deliverGoods(contract->tradeGood, sign*contract->amount);
  promisedToDeliver.deliverGoods(TradeGood::Money, -sign * contract->amount * contract->price);
}

TradeGood::TradeGood (string n, bool lastOne)
  : Enumerable<const TradeGood>(this, n, lastOne)
  , stickiness(0.2)
  , decay(0.00001)
  , consumption(1.0)
  , capital(0.9999)
{}

TradeGood::~TradeGood () {}

void TradeGood::initialise () {
  Enumerable<const TradeGood>::clear();
  Money = new TradeGood("money");  
  Labor = new TradeGood("labour");
}

void EconActor::unitTests () {
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    if ((*tg) == TradeGood::Money) throw string("exMoneyStart iterator should have skipped money.");
  }
}
