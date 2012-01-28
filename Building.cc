#include "Building.hh"
#include "PopUnit.hh"
#include "Hex.hh"
#include <algorithm> 
#include "MilUnit.hh" 
#include <cassert> 

const int Castle::maxGarrison = 2;
const int Castle::maxRecruits = 5; 

Castle::Castle (Hex* dat) 
  : Mirrorable<Castle>()
  , support(dat)
  , recruited(0)
{
  mirror->support = support;
  mirror->recruited = recruited; 
}

Castle::Castle (Castle* other) 
  : Mirrorable<Castle>(other)
  , support(0) // Sequence issues - this constructor is called before anything is initialised in real constructor
  , recruited(0)
{}

Castle::~Castle () {
  for (std::vector<MilUnit*>::iterator i = garrison.begin(); i != garrison.end(); ++i) {
    (*i)->destroyIfReal();
  }
  garrison.clear(); 
}

void Castle::setOwner (Player* p) {
  Building::setOwner(p);
  if (isReal()) mirror->setOwner(p); 
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
    addContent(this, ret, &Castle::addGarrison);
  }
  return ret; 
}

void Castle::addGarrison (MilUnit* p) {
  assert(p); 
  garrison.push_back(p);
  p->reinforce();
}

void Castle::setMirrorState () {
  mirror->setOwner(getOwner()); 
  mirror->support = support;
  mirror->recruited = recruited;
  mirror->garrison.clear();  
  for (std::vector<MilUnit*>::iterator unt = garrison.begin(); unt != garrison.end(); ++unt) {
    (*unt)->setMirrorState(); 
    mirror->garrison.push_back((*unt)->getMirror());
  }
}
