#include "EconActor.hh"
#include "StructUtils.hh"
#include "Logger.hh" 
#include <cassert>
#include <algorithm> 

TradeGood const* TradeGood::Money = 0;
TradeGood const* TradeGood::Labor = 0;

vector<double> Consumer::levelAmounts; 
vector<vector<MaslowNeed> > Consumer::hierarchy;

GoodsHolder::GoodsHolder ()
  : tradeGoods(TradeGood::numTypes(), 0)
{}

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

EconActor::EconActor ()
  : Numbered<EconActor>()
  , GoodsHolder()
{
  needs.resize(TradeGood::numTypes()); 
}

EconActor::~EconActor () {}

Market::Market ()
  : prices(TradeGood::numTypes(), 0)
{}

Market::~Market () {}

void Market::findPrices (vector<Bid>& wantToBuy, vector<Bid>& wantToSell) {
  Logger::logStream(DebugTrade) << "Entering findPrices with " << wantToBuy.size() << " " << wantToSell.size() << "\n"; 
  
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    vector<Bid> buys;
    vector<Bid> sell;
    for (vector<Bid>::iterator g = wantToBuy.begin(); g != wantToBuy.end(); ++g) {
      if ((*g).tradeGood != (*tg)) continue;
      buys.push_back(*g);
    }
    for (vector<Bid>::iterator g = wantToSell.begin(); g != wantToSell.end(); ++g) {
      if ((*g).tradeGood != (*tg)) continue;
      sell.push_back(*g);
    }

    if (0 == buys.size() * sell.size()) continue; 
    
    sort(buys.begin(), buys.end(), member_gt(&Bid::price));
    sort(sell.begin(), sell.end(), member_lt(&Bid::price));

    // Can we do business at all? 
    if (buys[0].price < sell[0].price) {
      prices[**tg] = sell[0].price;
      Logger::logStream(DebugTrade) << "Highest "
				    << (*tg)->getName()
				    << " bid "
				    << buys[0].price
				    << " less than lowest offer "
				    << sell[0].price
				    << ".\n"; 
      continue; 
    }

    double accBuys = 0;
    double accSell = 0;
    unsigned int sellIndex = 0; 
    unsigned int buysIndex = 0;
    while (true) {
      prices[**tg]  = (sell[sellIndex].price * sell[sellIndex].amount) + (buys[buysIndex].price * buys[buysIndex].amount);
      prices[**tg] /= (buys[buysIndex].amount + sell[sellIndex].amount);
      if (buys[buysIndex].amount - accBuys > sell[sellIndex].amount - accSell) {
	accBuys += sell[sellIndex].amount - accSell;
	accSell = 0;
	buysIndex++; 
      }
      else {
	accSell += buys[buysIndex].amount - accBuys;
	accBuys = 0;
	sellIndex++; 
      }
      if (sellIndex >= sell.size()) break;
      if (buysIndex >= buys.size()) break;
      if (sell[sellIndex].price > buys[buysIndex].price) break;
    }
    Logger::logStream(DebugTrade) << "Set price of " << (*tg)->getName() << " to " << prices[**tg] << "\n"; 
  }
}

void Market::trade (const vector<Bid>& wantToBuy, const vector<Bid>& wantToSell) {
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    vector<Bid> buys;
    vector<Bid> sell;
    for (vector<Bid>::const_iterator g = wantToBuy.begin(); g != wantToBuy.end(); ++g) {
      if ((*g).tradeGood != (*tg)) continue;
      buys.push_back(*g);
    }
    for (vector<Bid>::const_iterator g = wantToSell.begin(); g != wantToSell.end(); ++g) {
      if ((*g).tradeGood != (*tg)) continue;
      sell.push_back(*g);
    }

    if (0 == buys.size() * sell.size()) continue; 
    
    sort(buys.begin(), buys.end(), member_gt(&Bid::price));
    sort(sell.begin(), sell.end(), member_lt(&Bid::price));

    double accBuys = 0;
    double accSell = 0;
    unsigned int sellIndex = 0; 
    unsigned int buysIndex = 0;
    while (true) {
      if (prices[**tg] > buys[buysIndex].price) break;
      if (prices[**tg] < sell[sellIndex].price) break;
      
      if (buys[buysIndex].amount - accBuys > sell[sellIndex].amount - accSell) {
	double currAmount = sell[sellIndex].amount - accSell;
	buys[buysIndex].actor->deliverGoods((*tg),             currAmount);
	sell[sellIndex].actor->deliverGoods((*tg),            -currAmount);
	buys[buysIndex].actor->deliverGoods(TradeGood::Money, -currAmount*prices[**tg]);
	sell[sellIndex].actor->deliverGoods(TradeGood::Money,  currAmount*prices[**tg]);	
	accBuys += currAmount; 
	Logger::logStream(DebugTrade) << "Sold " << currAmount << " " << (*tg)->getName() << " at price " << prices[**tg] << "\n"; 
	accSell = 0;
	sellIndex++; 
      }
      else {
	double currAmount = buys[buysIndex].amount - accBuys;
	buys[buysIndex].actor->deliverGoods((*tg),             currAmount);
	sell[sellIndex].actor->deliverGoods((*tg),            -currAmount);
	buys[buysIndex].actor->deliverGoods(TradeGood::Money, -currAmount*prices[**tg]);
	sell[sellIndex].actor->deliverGoods(TradeGood::Money,  currAmount*prices[**tg]);	
	accSell += currAmount; 
	Logger::logStream(DebugTrade) << "Sold " << currAmount << " " << (*tg)->getName() << " at price " << prices[**tg] << "\n"; 
	accBuys = 0;
	buysIndex++; 
      }

      if (sellIndex >= sell.size()) break;
      if (buysIndex >= buys.size()) break;
    }
  }
}

void Consumer::setUtilities (vector<vector<Utility> >& needs, GoodsHolder const* const goods, double consumption) {
  double priorLevels = 1;
  double invConsumption = 1.0 / consumption; 
  for (unsigned int level = 0; level < hierarchy.size(); ++level) {
    double percentage = 0; 
    for (unsigned int i = 0; i < hierarchy[level].size(); ++i) {
      TradeGood const* const tg = hierarchy[level][i].tradeGood;
      double goodsPerPerson = goods->getAmount(tg) * invConsumption;
      double currAmount = hierarchy[level][i].amount;
      goodsPerPerson /= currAmount;
      percentage += log(goodsPerPerson + 1);
      double margin = log(goodsPerPerson + 2) - log(goodsPerPerson + 1); // Marginal utility from buying currAmount per consumer.
      margin *= priorLevels;
      needs[*tg].push_back(Utility(margin/currAmount, currAmount*consumption));
      margin = log(goodsPerPerson + 3) - log(goodsPerPerson + 2);
      margin *= priorLevels;      
      needs[*tg].push_back(Utility(margin/currAmount, currAmount*consumption));
      margin = log(goodsPerPerson + 4) - log(goodsPerPerson + 3);
      margin *= priorLevels;      
      needs[*tg].push_back(Utility(margin/currAmount, currAmount*consumption));       
    }
    priorLevels *= min(priorLevels, priorLevels * percentage / levelAmounts[level]);
    if (0.01 > priorLevels) break;
  }
}

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

void EconActor::getBids (const vector<double>& prices, vector<Bid>& wantToBuy, vector<Bid>& wantToSell) {
  double unitUtilityPrice = 1;
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    double accumulated = 0; 
    for (unsigned int j = 0; j < needs[**tg].size(); ++j) {
      if (accumulated + needs[**tg][j].margin >= getAmount(*tg)) {
	/*
	Logger::logStream(DebugTrade) << "Found "
				      << i
				      << " margin at "
				      << accumulated << " "
				      << goods[**tg] << " "
				      << needs[**tg][j].margin << " "
				      << needs[**tg][j].utility << " "
				      << prices[**tg] << " " 
				      << "\n";
	*/
	unitUtilityPrice = min(unitUtilityPrice, prices[**tg] / needs[**tg][j].utility);
	break; 
      }
      accumulated += getAmount(*tg);
    }
  }
  //Logger::logStream(DebugTrade) << getIdx() << " entering getBids with unit util price " << unitUtilityPrice << "\n"; 
  
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    double accumulated = 0; 
    for (unsigned int j = 0; j < needs[**tg].size(); ++j) {
      Bid currBid;
      currBid.tradeGood = (*tg);
      currBid.actor = this; 
      if (accumulated + needs[**tg][j].margin < getAmount(*tg)) {
	// Sell if the price is high enough
	currBid.amount = needs[**tg][j].margin;
	currBid.price  = needs[**tg][j].utility * unitUtilityPrice * 1.1;
	wantToSell.push_back(currBid);
	/*
	Logger::logStream(DebugTrade) << getIdx() << " sells " << i << " due to "
				      << needs[**tg][j].margin << " "
				      << goods[**tg] << " "
				      << needs[**tg][j].utility << " "
				      << accumulated << " "
				      << needs[**tg].size() << " "
				      << "\n";
	*/
      }
      else if ((accumulated < getAmount(*tg)) && (accumulated + needs[**tg][j].margin > getAmount(*tg))) {
	// Buy and sell
	currBid.amount = accumulated + needs[**tg][j].margin - getAmount(*tg); 
	currBid.price = needs[**tg][j].utility * unitUtilityPrice; 
	wantToBuy.push_back(currBid); 

	currBid.amount = getAmount(*tg) - accumulated;
	currBid.price  = needs[**tg][j].utility * unitUtilityPrice * 1.1;
	wantToSell.push_back(currBid);
      }
      else {
	// Buy
	currBid.amount = needs[**tg][j].margin; 
	currBid.price = needs[**tg][j].utility * unitUtilityPrice; 
	wantToBuy.push_back(currBid); 
      }
      accumulated += needs[**tg][j].margin;
    }
    if (accumulated < getAmount(*tg)) {
      // Final "leftovers" sell bid
      Bid finalBid;
      finalBid.tradeGood = (*tg);
      finalBid.actor = this;
      finalBid.amount = getAmount(*tg) - accumulated;
      finalBid.price = unitUtilityPrice * 0.1;
      wantToSell.push_back(finalBid);
    }
  }
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
