#include "MilUnit.hh"
#include "StructUtils.hh"
#include <algorithm>
#include <cmath> 
#include "Logger.hh" 

typedef std::vector<MilUnitElement*>::const_iterator CElmIter;
typedef std::vector<MilUnitElement*>::iterator ElmIter;
typedef std::vector<MilUnitElement*>::reverse_iterator RElmIter;
typedef std::vector<MilUnitElement*>::const_reverse_iterator CRElmIter; 

MilUnit::MilUnit ()
  : Unit()
  , Mirrorable<MilUnit>() 
  , weak(false)
  , rear(Hex::Left)
{}

MilUnit::MilUnit (MilUnit* other) 
  : Unit()
  , Mirrorable<MilUnit>(other) 
  , weak(other->weak)
  , rear(other->rear)
{}

MilUnit::~MilUnit () {}

void MilUnit::weaken () {
  weak = true;
}

void MilUnit::reinforce () {
  weak = false; 
}

void MilUnit::setMirrorState () {
  mirror->weak = weak;
  mirror->setOwner(getOwner()); 
}

double MilUnit::calcStrength (double tau, int MilUnitElement::*field) {
  std::sort(strength.begin(), strength.end(), deref<MilUnitElement>(member_lt(field)));
  double totalStrength = 0;
  double ret = 0;
  for (CElmIter i = strength.begin(); i != strength.end(); ++i) {
    double curr = (*i)->*field;
    double nums = (*i)->strength;
    ret += curr*(0.5 + (1.0 / tau)*(1 - exp(-tau*(nums - 0.5)))) * exp(-tau*totalStrength);
    totalStrength += nums;
  }
  return 1 + ret; 
}

const double invHalfPi = 0.6366197730950255;

void MilUnit::takeCasualties (double rate) {
  if (rate > 0.9) {
    // Unit disbands from the trauma.
    for (ElmIter i = strength.begin(); i != strength.end(); ++i) {
      delete (*i);
    }
    strength.clear(); 
  }
  else {
    for (ElmIter i = strength.begin(); i != strength.end(); ++i) {
      int loss = 1 + (int) floor((*i)->strength * rate + 0.5);
      (*i)->strength -= loss; 
    }
  }
}

void MilUnit::battleCasualties (MilUnit* const adversary) {
  int combatMod = getFightingModifier(adversary);
  double casualtyRate = invHalfPi * atan(combatMod); // Varies between -1 and 1, asymptotic
  casualtyRate *= 0.075; // Knock down to -0.075 to 0.075
  casualtyRate += 0.075; // Now varies between 0 and 15%
  takeCasualties(casualtyRate); 
}

void MilUnit::routCasualties (MilUnit* const adversary) {
  double ratio = std::max(1.0, calcStrength(0.001, &MilUnitElement::tacmob) - adversary->calcStrength(0.001, &MilUnitElement::fire));
  ratio /= std::max(1.0, adversary->calcStrength(0.001, &MilUnitElement::tacmob) - calcStrength(0.001, &MilUnitElement::fire));
  takeCasualties(0.2*exp(0.1*ratio)); 
}

int MilUnit::getScoutingModifier (MilUnit* const adversary) {
  double ratio = std::max(1.0, calcStrength(0.001, &MilUnitElement::tacmob) - adversary->calcStrength(0.001, &MilUnitElement::fire));
  ratio /= std::max(1.0, adversary->calcStrength(0.001, &MilUnitElement::tacmob) - calcStrength(0.001, &MilUnitElement::fire));

  if (ratio > 3.000) return 10;
  if (ratio > 2.000) return 5;
  if (ratio < 0.500) return -5;
  if (ratio < 0.333) return -10;
  return 0;
}

int MilUnit::getSkirmishModifier (MilUnit* const adversary) {
  return 0; 
}

int MilUnit::getFightingModifier (MilUnit* const adversary) {
  return 0; 
}
