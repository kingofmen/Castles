#include "EconActor.hh"
#include "StructUtils.hh"
#include "Logger.hh" 
#include <cassert>
#include <algorithm> 

const unsigned int EconActor::Money = 0;
const unsigned int EconActor::Labor = 1;
unsigned int EconActor::numGoods = 2; // Money and labour are hardcoded to exist.
vector<string> EconActor::goodNames; 
vector<EconActor*> EconActor::allActors; 
vector<double> Consumer::levelAmounts; 
vector<vector<MaslowNeed> > Consumer::hierarchy;

EconActor::EconActor ()
  : id(-1)
{
  goods = new double[numGoods];
  for (unsigned int i = 0; i < numGoods; ++i) {
    goods[i] = 0;
  }
  needs.resize(numGoods); 
}

EconActor::~EconActor () {
  allActors[id] = 0;
  delete[] goods;
}

Market::Market () {
  prices.resize(EconActor::numGoods);
}

Market::~Market () {}

void Market::findPrices (vector<Bid>& wantToBuy, vector<Bid>& wantToSell) {
  Logger::logStream(DebugTrade) << "Entering findPrices with " << wantToBuy.size() << " " << wantToSell.size() << "\n"; 
  
  for (unsigned int currGood = 1; currGood < EconActor::numGoods; ++currGood) {
    vector<Bid> buys;
    vector<Bid> sell;
    for (vector<Bid>::iterator g = wantToBuy.begin(); g != wantToBuy.end(); ++g) {
      if ((*g).good != currGood) continue;
      buys.push_back(*g);
    }
    for (vector<Bid>::iterator g = wantToSell.begin(); g != wantToSell.end(); ++g) {
      if ((*g).good != currGood) continue;
      sell.push_back(*g);
    }

    if (0 == buys.size() * sell.size()) continue; 
    
    sort(buys.begin(), buys.end(), member_gt(&Bid::price));
    sort(sell.begin(), sell.end(), member_lt(&Bid::price));

    // Can we do business at all? 
    if (buys[0].price < sell[0].price) {
      prices[currGood] = sell[0].price;
      Logger::logStream(DebugTrade) << "Highest "
				    << EconActor::getGoodName(currGood)
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
      prices[currGood]  = (sell[sellIndex].price * sell[sellIndex].amount) + (buys[buysIndex].price * buys[buysIndex].amount);
      prices[currGood] /= (buys[buysIndex].amount + sell[sellIndex].amount);
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
    Logger::logStream(DebugTrade) << "Set price of " << EconActor::getGoodName(currGood) << " to " << prices[currGood] << "\n"; 
  }
}

void Market::trade (const vector<Bid>& wantToBuy, const vector<Bid>& wantToSell) {
  for (unsigned int currGood = 1; currGood < EconActor::numGoods; ++currGood) {
    vector<Bid> buys;
    vector<Bid> sell;
    for (vector<Bid>::const_iterator g = wantToBuy.begin(); g != wantToBuy.end(); ++g) {
      if ((*g).good != currGood) continue;
      buys.push_back(*g);
    }
    for (vector<Bid>::const_iterator g = wantToSell.begin(); g != wantToSell.end(); ++g) {
      if ((*g).good != currGood) continue;
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
      if (prices[currGood] > buys[buysIndex].price) break;
      if (prices[currGood] < sell[sellIndex].price) break;
      
      if (buys[buysIndex].amount - accBuys > sell[sellIndex].amount - accSell) {
	double currAmount = sell[sellIndex].amount - accSell;
	buys[buysIndex].actor->deliverGoods(currGood,  currAmount);
	sell[sellIndex].actor->deliverGoods(currGood, -currAmount);
	buys[buysIndex].actor->deliverGoods(EconActor::Money,    -currAmount*prices[currGood]);
	sell[sellIndex].actor->deliverGoods(EconActor::Money,     currAmount*prices[currGood]);	
	accBuys += currAmount; 
	Logger::logStream(DebugTrade) << "Sold " << currAmount << " " << EconActor::getGoodName(currGood) << " at price " << prices[currGood] << "\n"; 
	accSell = 0;
	sellIndex++; 
      }
      else {
	double currAmount = buys[buysIndex].amount - accBuys;
	buys[buysIndex].actor->deliverGoods(currGood,  currAmount);
	sell[sellIndex].actor->deliverGoods(currGood, -currAmount);
	buys[buysIndex].actor->deliverGoods(EconActor::Money,    -currAmount*prices[currGood]);
	sell[sellIndex].actor->deliverGoods(EconActor::Money,     currAmount*prices[currGood]);	
	accSell += currAmount; 
	Logger::logStream(DebugTrade) << "Sold " << currAmount << " " << EconActor::getGoodName(currGood) << " at price " << prices[currGood] << "\n"; 
	accBuys = 0;
	buysIndex++; 
      }

      if (sellIndex >= sell.size()) break;
      if (buysIndex >= buys.size()) break;
    }
  }
}

/*
void Capitalist::setUtilities (vector<vector<Utility> >& needs, const vector<double>& prices, CivilBuilding const* const target) {
  // Consider Farmland, Forest, etc as converting input to output.
  // Assign inputs in two circumstances: Utility of output is higher,
  // or price of output is higher. 

  double totalInputUtility = 0;
  double totalOutputUtility = 0;
  double totalInputPrice = 0;
  double totalOutputPrice = 0; 
  for (unsigned int good = 1; good < EconActor::getNumGoods(); ++good) {
    
  }
}
*/

void Consumer::setUtilities (vector<vector<Utility> >& needs, double* goods, double consumption) {
  double priorLevels = 1;
  double invConsumption = 1.0 / consumption; 
  for (unsigned int level = 0; level < hierarchy.size(); ++level) {
    double percentage = 0; 
    for (unsigned int i = 0; i < hierarchy[level].size(); ++i) {
      unsigned int currGood = hierarchy[level][i].good;
      double goodsPerPerson = goods[currGood] * invConsumption;
      double currAmount = hierarchy[level][i].amount;
      goodsPerPerson /= currAmount;
      percentage += log(goodsPerPerson + 1);
      double margin = log(goodsPerPerson + 2) - log(goodsPerPerson + 1); // Marginal utility from buying currAmount per consumer.
      margin *= priorLevels;
      needs[currGood].push_back(Utility(margin/currAmount, currAmount*consumption));
      margin = log(goodsPerPerson + 3) - log(goodsPerPerson + 2);
      margin *= priorLevels;      
      needs[currGood].push_back(Utility(margin/currAmount, currAmount*consumption));
      margin = log(goodsPerPerson + 4) - log(goodsPerPerson + 3);
      margin *= priorLevels;      
      needs[currGood].push_back(Utility(margin/currAmount, currAmount*consumption));       
    }
    priorLevels *= min(priorLevels, priorLevels * percentage / levelAmounts[level]);
    if (0.01 > priorLevels) break;
  }
}

void EconActor::executeContracts () {
  for (Iter e = start(); e != final(); ++e) {
    for (vector<ContractInfo*>::iterator contract = (*e)->obligations.begin(); contract != (*e)->obligations.end(); ++contract) {
      if (!(*contract)->recipient) continue;
      double amountWanted = (*contract)->amount;
      switch ((*contract)->delivery) {
      default:
      case ContractInfo::Fixed:
	break;
      case ContractInfo::Percentage:
      case ContractInfo::SurplusPercentage:
	amountWanted *= (*e)->goods[(*contract)->good];
	break; 
      }
      Logger::logStream(Logger::Debug) << (*e)->getId() << " contract with " << (*contract)->recipient->getId() << " " << amountWanted << "\n"; 
      amountWanted = min((*e)->goods[(*contract)->good], amountWanted);
      (*contract)->recipient->deliverGoods((*contract)->good, amountWanted);
      (*e)->deliverGoods((*contract)->good, -amountWanted);
    }
  }
}

void EconActor::getBids (const vector<double>& prices, vector<Bid>& wantToBuy, vector<Bid>& wantToSell) {
  double unitUtilityPrice = 1; 
  for (unsigned int i = 1; i < numGoods; ++i) {
    double accumulated = 0; 
    for (unsigned int j = 0; j < needs[i].size(); ++i) {
      if (accumulated + needs[i][j].margin >= goods[i]) {
	/*
	Logger::logStream(DebugTrade) << "Found "
				      << i
				      << " margin at "
				      << accumulated << " "
				      << goods[i] << " "
				      << needs[i][j].margin << " "
				      << needs[i][j].utility << " "
				      << prices[i] << " " 
				      << "\n";
	*/
	unitUtilityPrice = min(unitUtilityPrice, prices[i] / needs[i][j].utility);
	break; 
      }
      accumulated += goods[i]; 
    }
  }
  //Logger::logStream(DebugTrade) << getId() << " entering getBids with unit util price " << unitUtilityPrice << "\n"; 
  
  for (unsigned int i = 1; i < numGoods; ++i) {
    double accumulated = 0; 
    for (unsigned int j = 0; j < needs[i].size(); ++j) {
      Bid currBid;
      currBid.good = i;
      currBid.actor = this; 
      if (accumulated + needs[i][j].margin < goods[i]) {
	// Sell if the price is high enough
	currBid.amount = needs[i][j].margin;
	currBid.price  = needs[i][j].utility * unitUtilityPrice * 1.1;
	wantToSell.push_back(currBid);
	/*
	Logger::logStream(DebugTrade) << getId() << " sells " << i << " due to "
				      << needs[i][j].margin << " "
				      << goods[i] << " "
				      << needs[i][j].utility << " "
				      << accumulated << " "
				      << needs[i].size() << " "
				      << "\n";
	*/
      }
      else if ((accumulated < goods[i]) && (accumulated + needs[i][j].margin > goods[i])) {
	// Buy and sell
	currBid.amount = accumulated + needs[i][j].margin - goods[i]; 
	currBid.price = needs[i][j].utility * unitUtilityPrice; 
	wantToBuy.push_back(currBid); 

	currBid.amount = goods[i] - accumulated;
	currBid.price  = needs[i][j].utility * unitUtilityPrice * 1.1;
	wantToSell.push_back(currBid);
      }
      else {
	// Buy
	currBid.amount = needs[i][j].margin; 
	currBid.price = needs[i][j].utility * unitUtilityPrice; 
	wantToBuy.push_back(currBid); 
      }
      accumulated += needs[i][j].margin;
    }
    if (accumulated < goods[i]) {
      // Final "leftovers" sell bid
      Bid finalBid;
      finalBid.good = i;
      finalBid.actor = this;
      finalBid.amount = goods[i] - accumulated;
      finalBid.price = unitUtilityPrice * 0.1;
      wantToSell.push_back(finalBid);
    }
  }
}

EconActor* EconActor::getById (int id) {
  if (0 > id) return 0;
  if (id >= (int) allActors.size()) return 0; 
  return allActors[id]; 
}

unsigned int EconActor::getIndex (string gName) {
  for (unsigned int i = 0; i < goodNames.size(); ++i) {
    if (goodNames[i] != gName) continue;
    return i;
  }
  Logger::logStream(DebugStartup) << "Bad name " << gName << "\n"; 
  assert(false);
  return goodNames.size(); 
}

void EconActor::setAllUtils () {
  for (Iter e = start(); e != final(); ++e) (*e)->setUtilities(); 
}

void EconActor::setUtilities () {} // Do nothing by default - override in subclasses. 

void EconActor::production () {
  for (Iter e = start(); e != final(); ++e) (*e)->produce(); 
}

