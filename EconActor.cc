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
  needs = new double[numGoods];
  for (unsigned int i = 0; i < numGoods; ++i) {
    goods[i] = 0;
    needs[i] = 0;
  }
}

EconActor::~EconActor () {
  allActors[id] = 0;
  delete[] goods;
  delete[] needs; 
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

    sort(buys.begin(), buys.end(), member_gt(&Bid::price));
    sort(sell.begin(), sell.end(), member_lt(&Bid::price));

  }
}

void EconActor::getBids (const vector<double>& prices, vector<Bid>& wantToBuy, vector<Bid>& wantToSell) {
  for (unsigned int i = 1; i < numGoods; ++i) {
    Bid currBid;
    currBid.good = i;
    currBid.price = prices[i];
    if (goods[i] > needs[i]) {
      currBid.amount = (goods[i] - needs[i]);
      wantToSell.push_back(currBid);
    }
    else {
      currBid.amount = (needs[i] - goods[i]);
      wantToBuy.push_back(currBid);
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

