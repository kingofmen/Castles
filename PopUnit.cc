#include "PopUnit.hh"
#include <cmath>
#include <cassert> 

PopUnit::PopUnit ()
  : Unit()
  , recruited(0)
{}
PopUnit::~PopUnit () {}
Unit::Unit () :
  player(0)
{}
Unit::~Unit () {}

double PopUnit::recruitsAvailable () const {
  return 1000 - recruited; 
}

bool PopUnit::recruit (double number) {
  if (number + recruited > 500) return false;
  recruited += number;
  return true; 
}

double PopUnit::production () const {
  double recruitEffect = 750*pow(std::min(500.0, recruited)*0.002, 2);
  return 1000 - recruitEffect; 
}

double PopUnit::growth (Hex::TerrainType t) {
  double growthFactor = 0.1;
  switch (t) {
  case Hex::Mountain: growthFactor = 0.05; break;
  case Hex::Hill:     growthFactor = 0.07; break;
  case Hex::Plain:    growthFactor = 0.10; break;
  case Hex::Forest:   growthFactor = 0.08; break;
  case Hex::Ocean:
  case Hex::Unknown:
  default:
    growthFactor = 0;
    break; 
  }
  double newPeople = growthFactor*(1000 - recruited);
  if (recruited > newPeople) {
    recruited -= newPeople;
    newPeople = 0;
  }
  else {
    newPeople -= recruited;
    recruited = 0;
  }
  return newPeople; 
}
 

