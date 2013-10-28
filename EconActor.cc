#include "EconActor.hh"
#include <cassert>
#include "StructUtils.hh"
#include <algorithm> 

const unsigned int EconActor::Money = 0;
const unsigned int EconActor::Labor = 1;
unsigned int EconActor::numGoods = 2; // Money and labour are hardcoded to exist.
vector<string> EconActor::goodNames; 
vector<EconActor*> EconActor::allActors; 

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
      continue; 
    }

    double accBuys = 0;
    double accSell = 0;
    unsigned int sellIndex = 0; 
    unsigned int buysIndex = 0;
    while (true) {
      prices[currGood]  = (sell[sellIndex].price * sell[sellIndex].amount) + (buys[buysIndex].price * buy[buysIndex].amount);
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
      if (sell[sellIndex].price > buys[buysIndex]) break;
    }
  }
}

void EconActor::getBids (const vector<double>& prices, vector<Bid>& wantToBuy, vector<Bid>& wantToSell) {
  double unitUtilityPrice = 1; 
  
  for (unsigned int i = 1; i < numGoods; ++i) {
    double accumulated = 0; 
    for (unsigned int j = 0; j < needs[i].size(); ++i) {
      Bid currBid;
      currBid.good = i;      
      if (accumulated + needs[i][j].margin < goods[i]) {
	// Sell if the price is high enough
	currBid.amount = needs[i][j].margin;
	currBid.price  = needs[i][j].utility * unitUtilityPrice * 1.1;
	wantToSell.push_back(currBid);
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
  assert(false);
  return goodNames.size(); 
}

