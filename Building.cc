#include "Building.hh"
#include "PopUnit.hh"
#include "Hex.hh"
#include <algorithm> 
#include "MilUnit.hh"
#include "UtilityFunctions.hh"
#include <cassert> 
#include "Calendar.hh" 

int Village::maxPopulation = 1; 
vector<double> Village::baseMaleMortality(AgeTracker::maxAge);
vector<double> Village::baseFemaleMortality(AgeTracker::maxAge);
vector<double> Village::pairChance(AgeTracker::maxAge);
vector<double> Village::fertility(AgeTracker::maxAge);
vector<double> Village::products(AgeTracker::maxAge);
vector<double> Village::consume(AgeTracker::maxAge);
vector<double> Village::recruitChance(AgeTracker::maxAge);
double Village::femaleProduction = 0.5;
double Village::femaleConsumption = 0.90;
double Village::femaleSurplusEffect = -2.0;
double Village::femaleSurplusZero = 1.0;
double Castle::siegeModifier = 10; 

int Farmland::_labourToSow    = 1;
int Farmland::_labourToPlow   = 10;
int Farmland::_labourToClear  = 100;
int Farmland::_labourToWeed   = 3;
int Farmland::_labourToReap   = 10;
int Farmland::_cropsFrom3     = 1000;
int Farmland::_cropsFrom2     = 500;
int Farmland::_cropsFrom1     = 100;

Castle::Castle (Hex* dat, Line* lin) 
  : Mirrorable<Castle>()
  , support(dat)
  , location(lin)
  , recruitType(0) 
{
  recruitType = *(MilUnitTemplate::begin());
  setMirrorState();
}

Castle::Castle (Castle* other) 
  : Mirrorable<Castle>(other)
  , support(0) // Sequence issues - this constructor is called before anything is initialised in real constructor
{}

Castle::~Castle () {
  for (std::vector<MilUnit*>::iterator i = garrison.begin(); i != garrison.end(); ++i) {
    (*i)->destroyIfReal();
  }
  garrison.clear(); 
}

void Castle::addGarrison (MilUnit* p) {
  assert(p); 
  garrison.push_back(p);
  p->setLocation(0);
  p->setExtMod(siegeModifier); // Fortification bonus 
}

void Castle::callForSurrender (MilUnit* siegers, Outcome out) {
  if (0 == garrison.size()) {
    setOwner(siegers->getOwner());
    return;
  }

  BattleResult assault = siegers->attack(garrison[0], out);
  if (VictoGlory == assault.attackerSuccess) {
    garrison[0]->destroyIfReal();
    garrison[0] = garrison.back();
    garrison.pop_back();
    if (0 == garrison.size()) {
      setOwner(siegers->getOwner());
      return;
    }
  }
}

void Castle::endOfTurn () {
  support->getVillage()->demandSupplies(&taxExtraction);
  supplies += taxExtraction.delivered;
  taxExtraction.delivered = 0;
}

void Castle::supplyGarrison () {
  if (0 == garrison.size()) return; 
  double totalRequired = 0.001; 
  for (vector<MilUnit*>::iterator m = garrison.begin(); m != garrison.end(); ++m) {
    totalRequired += (*m)->getPrioritisedSuppliesNeeded();
  }
  double fraction = supplies / totalRequired;
  if (fraction > 1) fraction = 1; 
  for (vector<MilUnit*>::iterator m = garrison.begin(); m != garrison.end(); ++m) {
    double required = (*m)->getPrioritisedSuppliesNeeded();
    required *= fraction;
    supplies -= required;
    (*m)->addSupplies(required); 
  }
}

MilUnit* Castle::removeGarrison () {
 MilUnit* ret = garrison.back();
 garrison.pop_back();
 ret->dropExtMod(); // No longer gets fortification bonus. 
 return ret;
 }

double Castle::removeSupplies (double amount) {
  if (amount > supplies) amount = supplies;
  supplies -= amount;
  return amount; 
}

MilUnit* Castle::removeUnit (MilUnit* dat) {
  std::vector<MilUnit*>::iterator target = std::find(garrison.begin(), garrison.end(), dat);
  if (target == garrison.end()) return 0;
  MilUnit* ret = (*target);
  garrison.erase(target); 
  return ret; 
}

void Castle::recruit (Outcome out) {
  Logger::logStream(DebugStartup) << "Recruiting: " << this << " " << (void*) &recruitType << " " << (void*) recruitType << "\n"; 
  if (!recruitType) recruitType = *(MilUnitTemplate::begin());
  MilUnit* target = (garrison.size() > 0 ? garrison[0] : new MilUnit());
  int newSoldiers = support->recruit(getOwner(), recruitType, target, out);
  //if (isReal()) Logger::logStream(Logger::Debug) << "Recruited " << newSoldiers << " " << recruitType->name << "\n"; 
  if (0 == garrison.size()) {
    if (0 == newSoldiers) delete target;
    else {
      addGarrison(target); 
      target->setOwner(getOwner()); 
    }
  }
}

void Castle::setOwner (Player* p) {
  Building::setOwner(p);
  if (isReal()) mirror->setOwner(p); 
  for (std::vector<MilUnit*>::iterator u = garrison.begin(); u != garrison.end(); ++u) {
    (*u)->setOwner(p); 
  }
}

void Castle::setMirrorState () {
  mirror->setOwner(getOwner()); 
  mirror->support = support;
  mirror->location = location; 
  mirror->supplies = supplies;
  mirror->recruitType = recruitType;

  mirror->garrison.clear();  
  for (std::vector<MilUnit*>::iterator unt = garrison.begin(); unt != garrison.end(); ++unt) {
    (*unt)->setMirrorState(); 
    mirror->garrison.push_back((*unt)->getMirror());
  }
}

Village::Village ()
  : Building()
  , Mirrorable<Village>()
  , milTrad(0)
  , foodMortalityModifier(1)
{
  milTrad = new MilitiaTradition();
}

Village::Village (Village* other)
  : Mirrorable<Village>(other) 
{}

Village::~Village () {}

const MilUnitGraphicsInfo* Village::getMilitiaGraphics () const {
  return milTrad ? milTrad->militia->getGraphicsInfo() : 0;
}

MilitiaTradition::MilitiaTradition ()
  : Mirrorable<MilitiaTradition>()
{
  militia = new MilUnit();
  setMirrorState();
}

MilitiaTradition::~MilitiaTradition () {}

MilitiaTradition::MilitiaTradition (MilitiaTradition* other)
  : Mirrorable<MilitiaTradition>(other)
{}

void MilitiaTradition::increaseTradition (MilUnitTemplate const* target) { 
  if (!target) target = getKeyByWeight<MilUnitTemplate const* const>(militiaStrength);
  if (target) militiaStrength[target]++;
}

void MilitiaTradition::decayTradition () {
  for (map<MilUnitTemplate const* const, int>::iterator i = militiaStrength.begin(); i != militiaStrength.end(); ++i) {
    int loss = convertFractionToInt((*i).second * (*i).first->militiaDecay * MilUnitTemplate::getDrillEffect(drillLevel));
    militiaStrength[(*i).first] -= loss;
  }
}

double MilitiaTradition::getRequiredWork () {
  double ret = 0; 
  for (map<MilUnitTemplate const* const, int>::iterator i = militiaStrength.begin(); i != militiaStrength.end(); ++i) {
    ret += drillLevel * (*i).first->recruit_speed * (*i).first->militiaDrill * (*i).second; 
  }
  return ret; 
}

int MilitiaTradition::getUnitTypeAmount (MilUnitTemplate const* const ut) const {
  if (militiaStrength.find(ut) == militiaStrength.end()) return 0; 
  return ut->recruit_speed * militiaStrength.at(ut); // Use 'at' for const-ness. 
}


void MilitiaTradition::setMirrorState () {
  militia->setMirrorState();  
  mirror->militia = militia->getMirror();
  for (map<MilUnitTemplate const* const, int>::iterator i = militiaStrength.begin(); i != militiaStrength.end(); ++i) {
    mirror->militiaStrength[(*i).first] = (*i).second;
  }
  mirror->drillLevel = drillLevel; 
}


double Village::adjustedMortality (int age, bool male) const {
  if (male) return baseMaleMortality[age]*foodMortalityModifier;
  return baseFemaleMortality[age]*foodMortalityModifier;
}

void Village::endOfTurn () {
  eatFood();  
  Calendar::Season currSeason = Calendar::getCurrentSeason();
  if (Calendar::Winter != currSeason) return;

  males.age();
  women.age();
  
  for (int i = 1; i < AgeTracker::maxAge; ++i) {
    double fracLoss = adjustedMortality(i, true) * males.getPop(i);
    males.addPop(-convertFractionToInt(fracLoss), i);
    
    fracLoss = adjustedMortality(i, false) * women.getPop(i);
    women.addPop(-convertFractionToInt(fracLoss), i);
  }

  milTrad->decayTradition(); 

  MilUnitGraphicsInfo* milGraph = (MilUnitGraphicsInfo*) milTrad->militia->getGraphicsInfo();  
  if (milGraph) milGraph->updateSprites(milTrad); 
  
  // Simplifying assumptions:
  // No man ever impregnates an older woman
  // Male fertility is constant, only female fertility matters
  // No man gets more than one woman pregnant in a year 
  // Young men get the first chance at single women

  double popIncrease = 0;
  vector<int> takenWomen(AgeTracker::maxAge);
  for (int mAge = 16; mAge < AgeTracker::maxAge; ++mAge) {
    for (int fAge = 16; fAge <= mAge; ++fAge) {
      int availableWomen = women.getPop(fAge) - takenWomen[fAge];
      if (1 > availableWomen) continue; 
      double pairs = min(availableWomen, males.getPop(mAge)) * pairChance[mAge - fAge];
      double pregnancies = pairs * fertility[fAge];
      takenWomen[fAge] += (int) floor(pairs);
      popIncrease += convertFractionToInt(pregnancies); 
    }
  }

  males.addPop((int) floor(0.5 * popIncrease + 0.5), 0);
  women.addPop((int) floor(0.5 * popIncrease + 0.5), 0);

  updateMaxPop(); 
}

void Village::demobMilitia () {
  if (!milTrad) return;
  milTrad->militia->demobilise(males); 
}

MilUnit* Village::raiseMilitia () {
  for (map<MilUnitTemplate const* const, int>::iterator i = milTrad->militiaStrength.begin(); i != milTrad->militiaStrength.end(); ++i) {
    for (int j = 0; j < (*i).second; ++j) {
      produceRecruits((*i).first, milTrad->militia, Neutral); 
    }
  }

  if (milTrad->isReal()) {
    Logger::logStream(Logger::Game) << "Militia raised: \n"
				    << milTrad->militia->getGraphicsInfo()->strengthString("  ");
  }

  milTrad->increaseTradition();  
  return milTrad->militia; 
}

Farmland::Farmland () 
  : Building()
  , Mirrorable<Farmland>()
{
  for (int i = 0; i < NumStatus; ++i) fields[i] = 0; 
}

Farmland::~Farmland () {}

Farmland::Farmland (Farmland* other)
  : Mirrorable<Farmland>(other) 
{}

void Village::setMirrorState () {
  women.setMirrorState();
  males.setMirrorState();
  milTrad->setMirrorState();
  mirror->milTrad = milTrad->getMirror();
  mirror->foodMortalityModifier = foodMortalityModifier; 
  mirror->setOwner(getOwner());
}

void Farmland::setMirrorState () {
  mirror->setOwner(getOwner());
  for (int i = 0; i < NumStatus; ++i) mirror->fields[i] = fields[i];
}


double Village::production () const {
  double ret = 0;
  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    ret += (males.getPop(i) + femaleProduction*women.getPop(i)) * products[i]; 
  }

  ret -= milTrad->getRequiredWork(); 
  return ret;
}

double Village::consumption () const {
  double ret = 0;
  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    ret += consume[i]*(males.getPop(i) + femaleConsumption*women.getPop(i));
  }
  return ret; 
}

void Farmland::devastate (int devastation) {
  while (devastation > 0) {
    if      (fields[Ripe3] > 0) {fields[Ripe3]--; fields[Ripe2]++;}
    else if (fields[Ripe2] > 0) {fields[Ripe2]--; fields[Ripe1]++;}
    else if (fields[Ripe1] > 0) {fields[Ripe1]--; fields[Ready]++;}
    devastation--;
  }
}

void Village::demandSupplies (ContractInfo* taxes) {
  double amount = 0;
  double needed = consumption() * Calendar::turnsToAutumn(); 
  switch (taxes->delivery) {
  case ContractInfo::Fixed:
  default: 
    amount = taxes->amount;
    break;
  case ContractInfo::Percentage:
    amount = supplies*taxes->amount;
    break;
  case ContractInfo::SurplusPercentage:
    amount = (supplies - needed);
    if (amount < 0) amount = 0;
    else amount *= taxes->amount; 
    break;
  }

  
  double starvingness = (supplies - amount);
  starvingness /= needed;
  if (starvingness < 1) {
    // Hide the food!
    amount *= ((atan(starvingness) + 3*0.25*M_PI) / (M_PI_2)); // Between 0.5 (starvingness = negative inf) and 1 (starvingness = 1)
  }

  if (amount > supplies) amount = supplies;
  supplies -= amount;
  taxes->delivered = amount; 
}

void Farmland::endOfTurn () {
  workFields();
}

void Village::eatFood () {
  double needed = consumption();
  double available = supplies;
  available /= Calendar::turnsToAutumn();
  double fraction = available/needed;
  if (fraction > 2) fraction = 2;
  supplies -= fraction * needed;
  foodMortalityModifier = 1.0 / std::max(0.1, fraction); 
}

void Farmland::workFields () {  
  Calendar::Season currSeason = Calendar::getCurrentSeason();
  int total = getAssignedLand(); 
  double availableLabour = 1000; //production(); 
  
  switch (currSeason) {
  default:
  case Calendar::Winter:
    // Cleanup.

    // Used land becomes 'clear' for next year.
    fields[Ended] += fields[Ripe3];
    fields[Ended] += fields[Ripe2];
    fields[Ended] += fields[Ripe1];
    fields[Ended] += fields[Sowed];
    fields[Ended] += fields[Ready];
    fields[Ripe3] = fields[Ripe2] = fields[Ripe1] = fields[Sowed] = fields[Ready] = 0;

    // Fallow acreage grows over.
    fields[Clear] = fields[Ended];
    fields[Ended] = 0; 

    break; 

  case Calendar::Spring:
    // In spring we clear new fields, plow and sow existing ones.
    while (true) {
      if ((availableLabour >= _labourToSow) && (fields[Ready] > 0)) {
	fields[Ready]--;
	fields[Sowed]++;
	availableLabour -= _labourToSow;
      }
      else if ((availableLabour >= _labourToPlow) && (fields[Clear] > 0)) {
	fields[Clear]--;
	fields[Ready]++;
	availableLabour -= _labourToPlow;
      }
      else if ((availableLabour >= _labourToClear) && (total - fields[Clear] - fields[Ready] - fields[Sowed] - fields[Ripe1] - fields[Ripe2] - fields[Ripe3] - fields[Ended] > 0)) {
	fields[Clear]++;
	availableLabour -= _labourToClear;
      }
      else break; 
    }
    
    break;

  case Calendar::Summer:
    // In summer the crops ripen and we fight the weeds.
    // If there isn't enough labour to tend the crops, they
    // degrade; if the weather is bad they don't advance.
    {
      double weatherModifier = 1; // TODO: Insert weather-getting code here
      int untendedRipe1 = fields[Ripe1]; fields[Ripe1] = 0;
      int untendedRipe2 = fields[Ripe2]; fields[Ripe2] = 0;
      int untendedRipe3 = fields[Ripe3]; fields[Ripe3] = 0;
      while (availableLabour >= _labourToWeed * weatherModifier) {
	availableLabour -= _labourToWeed * weatherModifier;	
	if (fields[Sowed] > 0) {
	  fields[Sowed]--;
	  fields[Ripe1]++;
	}
	else if (untendedRipe1 > 0) {
	  untendedRipe1--;
	  fields[Ripe2]++;
	}
	else if (untendedRipe2 > 0) {
	  untendedRipe2--;
	  fields[Ripe3]++;
	}
	else if (untendedRipe3 > 0) {
	  untendedRipe3--;
	  fields[Ripe3]++;
	}
	else break; 
      }
      fields[Sowed] += untendedRipe1;
      fields[Ripe1] += untendedRipe2;
      fields[Ripe2] += untendedRipe3;            
    }
    
    break;

  case Calendar::Autumn:
    {
      // In autumn we harvest.
      int harvest = min(fields[Ripe3], (int) floor(availableLabour / _labourToReap));
      availableLabour -= harvest * _labourToReap;
      supplies += _cropsFrom3 * harvest;
      fields[Ripe3] -= harvest;
      fields[Ended] += harvest;

      harvest = min(fields[Ripe2], (int) floor(availableLabour / _labourToReap));
      availableLabour -= harvest * _labourToReap;
      supplies += _cropsFrom2 * harvest;
      fields[Ripe2] -= harvest;
      fields[Ended] += harvest;

      harvest = min(fields[Ripe1], (int) floor(availableLabour / _labourToReap));
      availableLabour -= harvest * _labourToReap;
      supplies += _cropsFrom1 * harvest;
      fields[Ripe1] -= harvest;
      fields[Ended] += harvest;      
    }
    
    break; 
  }  
}



int Village::produceRecruits (MilUnitTemplate const* const recruitType, MilUnit* target, Outcome dieroll) {
  double luckModifier = 1.0;
  switch (dieroll) {
  case VictoGlory: luckModifier = 1.50; break;
  case Good:       luckModifier = 1.25; break;
  case Bad:        luckModifier = 0.90; break;
  case Disaster:   luckModifier = 0.75; break;
  case Neutral:
  default:
    break; 
  }

  static AgeTracker recruits;
  recruits.clear(); 
  int recruited = 0;
  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    int numMen = males.getPop(i);
    if (1 > numMen) continue; 
    double femaleSurplus = women.getPop(i);
    femaleSurplus /= numMen; 
    femaleSurplus -= femaleSurplusZero; 

    int curr = convertFractionToInt(numMen * recruitChance[i] * luckModifier * exp(femaleSurplusEffect*femaleSurplus));
    if (curr > numMen) curr = numMen;
    if (recruited + curr > recruitType->recruit_speed) curr = recruitType->recruit_speed - recruited;
    if (1 > curr) continue;
    recruits.addPop(curr, i);
    males.addPop(-curr, i);
    recruited += curr; 
    if (recruited >= recruitType->recruit_speed) break; 
  }

  target->addElement(recruitType, recruits); 

  return recruited; 
}


