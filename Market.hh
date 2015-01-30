#ifndef MARKET_HH
#define MARKET_HH

#include "EconActor.hh"
#include "UtilityFunctions.hh"

struct MarketBid {
  MarketBid(TradeGood const* tg, double atb, EconActor* b) : tradeGood(tg), amountToBuy(atb), bidder(b) {}
  
  TradeGood const* tradeGood;
  double amountToBuy;
  EconActor* bidder;
};

struct MarketContract {
  MarketContract (MarketBid* one, MarketBid* two, double p);
  
  void   clear ();
  double execute ();
  void   pay ();

  EconActor* recipient;
  EconActor* producer;
  const TradeGood* tradeGood;
  double amount;
  double delivered;
  double price;  
};

class Market {
  friend class HexGraphicsInfo;
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
  void makeContracts(vector<MarketBid*>& bids, vector<MarketBid*>& notMatched);
  void adjustPrices(vector<MarketBid*>& notMatched);
  
  GoodsHolder prices;
  GoodsHolder volume;
  GoodsHolder demand;
  vector<MarketContract*> contracts;
  vector<EconActor*> participants;
};

#endif
