#ifndef MARKET_HH
#define MARKET_HH

#include "EconActor.hh"
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

  void   clear ();
  double execute ();
  bool   isValid () const {return ((remainingTime > 0) && (accumulatedMissing < amount));}
  void   pay ();

  EconActor* recipient;
  EconActor* producer;
  const TradeGood* tradeGood;
  double amount;
  double delivered;
  double price;
  unsigned int remainingTime;
  double accumulatedMissing;
};

class Market {
  friend class HexGraphicsInfo;
  friend class StaticInitialiser;
public:
  Market ();
  ~Market () {}

  void holdMarket ();
  void registerParticipant (EconActor* ea);
  void unRegisterParticipant (EconActor* ea);
  double getDemand (TradeGood const* const tg) const {return demand.getAmount(tg);}
  double getPrice  (TradeGood const* const tg) const {return prices.getAmount(tg);}
  double getVolume (TradeGood const* const tg) const {return volume.getAmount(tg);}

  static void unitTests ();
private:
  void adjustPrices(vector<MarketBid*>& notMatched);
  void executeContracts ();
  void makeContracts(vector<MarketBid*>& bids, vector<MarketBid*>& notMatched);
  void normalisePrices ();
  
  GoodsHolder prices;
  GoodsHolder volume;
  GoodsHolder demand;
  vector<MarketContract*> contracts;
  vector<EconActor*> participants;
};

#endif
