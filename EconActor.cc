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

void GoodsHolder::clear () {
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] = 0;
  }
}

void GoodsHolder::setAmounts (GoodsHolder const* const gh) {
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] = gh->getAmount(*tg);
  }
}

void GoodsHolder::setAmounts (const GoodsHolder& gh) {
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] = gh.getAmount(*tg);
  }
}

void GoodsHolder::operator+= (const GoodsHolder& other) {
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] += other.tradeGoods[**tg];
  }
}

void GoodsHolder::operator-= (const GoodsHolder& other) {
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    tradeGoods[**tg] -= other.tradeGoods[**tg];
  }
}

EconActor::EconActor ()
  : Numbered<EconActor>()
  , GoodsHolder()
  , owner(0)
{}

EconActor::~EconActor () {}

void ContractInfo::execute () const {
  if (!recipient) return;
  if (!source) return;
  double amountWanted = amount;
  switch (delivery) {
  default:
  case Fixed:
    break;
  case Percentage:
  case SurplusPercentage:
    amountWanted *= source->getAmount(tradeGood);
    break;
  }
  Logger::logStream(Logger::Debug) << source->getIdx() << " contract with " << recipient->getIdx() << " " << amountWanted << "\n"; 
  amountWanted = min(source->getAmount(tradeGood), amountWanted);
  recipient->deliverGoods(tradeGood, amountWanted);
  source->deliverGoods(tradeGood, -amountWanted);
}

double EconActor::availableCredit (EconActor* const applicant) const {
  if ((isOwnedBy(applicant)) || (applicant->isOwnedBy(this))) return 1e7;
  static const double maxCredit = 100;
  double amountAvailable = maxCredit - borrowers.find(applicant)->second;
  if (amountAvailable < 0) return 0;
  return amountAvailable;
}

void EconActor::dunAndPay () {
  for (map<EconActor* const, double>::iterator borrower = borrowers.begin(); borrower != borrowers.end(); ++borrower) {
    EconActor* const victim = (*borrower).first;
    double amountToPay = min((*borrower).second, 0.5*victim->getAmount(TradeGood::Money));
    getPaid(victim, amountToPay);
  }

  soldThisTurn.clear();
  
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

double EconActor::produceForContract (TradeGood const* const tg, double amount) {
  amount = min(amount, getAmount(tg));
  deliverGoods(tg, -amount);
  registerSale(tg, amount);
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
