#ifndef MARKET_HH
#define MARKET_HH

#include "EconActor.hh"
#include "Mirrorable.hh"
#include "UtilityFunctions.hh"

struct MarketBid {
  MarketBid(TradeGood const* tg, double atb, EconActor* b, unsigned int d = 1) : tradeGood(tg), amountToBuy(atb), bidder(b), duration(d) {}
  
  TradeGood const* tradeGood;
  double amountToBuy;
  EconActor* bidder;
  unsigned int duration;
};

struct MarketContract {
  MarketContract (MarketBid* one, MarketBid* two, double p, unsigned int duration);
  ~MarketContract ();

  void    clear ();
  double  execute ();
  doublet execute (MarketContract* other);
  bool    isValid () const {return ((remainingTime > 0) && (accumulatedMissing < amount));}
  void    pay ();

  EconActor* recipient;
  EconActor* producer;
  const TradeGood* tradeGood;
  double amount;
  double cashPaid;
  double delivered;
  double price;
  unsigned int remainingTime;
  double accumulatedMissing;

private:
  double deliver (double amountWanted);
  double remaining () const {return amount - delivered;}
};

class Market : public Mirrorable<Market> {
  friend class HexGraphicsInfo;
  friend class StaticInitialiser;
  friend class Mirrorable<Market>;
public:
  Market ();
  ~Market () {}

  void holdMarket ();
  void registerParticipant (EconActor* ea);
  void registerProduction  (TradeGood const* tg, double amount) {produced.deliverGoods(tg, amount);}
  void registerConsumption (TradeGood const* tg, double amount) {consumed.deliverGoods(tg, amount);}
  void unRegisterParticipant (EconActor* ea);
  double getConsumed (TradeGood const* const tg) const {return consumed.getAmount(tg);}
  double getDemand   (TradeGood const* const tg) const {return demand.getAmount(tg);}
  double getPrice    (TradeGood const* const tg) const {return prices.getAmount(tg);}
  double getProduced (TradeGood const* const tg) const {return produced.getAmount(tg);}
  double getVolume   (TradeGood const* const tg) const {return volume.getAmount(tg);}
  virtual void setMirrorState ();

  static void unitTests ();
private:
  Market (Market* other);

  void adjustPrices(vector<MarketBid*>& notMatched);
  void executeContracts ();
  void makeContracts(vector<MarketBid*>& bids, vector<MarketBid*>& notMatched);
  void normalisePrices ();
  
  GoodsHolder prices;
  GoodsHolder volume;
  GoodsHolder demand;
  GoodsHolder produced;
  GoodsHolder consumed;
  vector<MarketContract*> contracts;
  vector<EconActor*> participants;
};

#endif
