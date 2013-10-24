#include "EconActor.hh"
#include <cassert> 

const unsigned int EconActor::Money = 0;
const unsigned int EconActor::Labor = 1;
int EconActor::numGoods = 2; // Money and labour are hardcoded to exist.
vector<string> EconActor::goodNames; 
vector<EconActor*> EconActor::allActors; 

EconActor::EconActor ()
  : id(-1)
{
  goods = new double[numGoods];
  for (int i = 0; i < numGoods; ++i) goods[i] = 0; 
}

EconActor::~EconActor () {
  allActors[id] = 0;
  delete[] goods; 
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
