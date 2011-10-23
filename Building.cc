#include "Building.hh"
#include "PopUnit.hh"
#include "Hex.hh"
#include <algorithm> 

const int Castle::maxGarrison = 2;
const int Castle::maxRecruits = 5; 

Castle::Castle (Hex* dat) 
  : support(dat)
  , recruited(0)
{
  static bool makeMirror = true; 
  if (makeMirror) {
    makeMirror = false;
    mirror = new Castle(dat);
    makeMirror = true; 
  }
}

Castle::~Castle () {
  if (getMirror()) {
    for (std::vector<MilUnit*>::iterator i = garrison.begin(); i != garrison.end(); ++i) {
      delete (*i); 
    }
  }
  garrison.clear(); 
}

void Castle::setOwner (Player* p) {
  Building::setOwner(p); 
  for (std::vector<MilUnit*>::iterator u = garrison.begin(); u != garrison.end(); ++u) {
    (*u)->setOwner(p); 
  }
}

MilUnit* Castle::removeUnit (MilUnit* dat) {
  std::vector<MilUnit*>::iterator target = std::find(garrison.begin(), garrison.end(), dat);
  if (target == garrison.end()) return 0;
  MilUnit* ret = (*target);
  garrison.erase(target); 
  return ret; 
}

MilUnit* Castle::recruit () {
  recruited++;
  MilUnit* ret = NULL; 
  if (recruited >= 5) {
    recruited = 0;
    ret = new MilUnit();
    ret->setOwner(getOwner());
    garrison.push_back(ret); 
  }
  return ret; 
}

void Castle::unRecruit () {
  recruited--;
  if (recruited < 0) recruited = 4; 
}

void Castle::addGarrison (MilUnit* p) {
  garrison.push_back(p);
  p->reinforce();
}

void Castle::setMirrorState () {
  mirror->setOwner(getOwner()); 
  mirror->garrison.clear();
  mirror->support = support;
  mirror->recruited = recruited;
  for (std::vector<MilUnit*>::iterator unt = garrison.begin(); unt != garrison.end(); ++unt) {
    (*unt)->setMirrorState(); 
    mirror->garrison.push_back((*unt)->getMirror());
  }
}
