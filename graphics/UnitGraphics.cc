#include "UnitGraphics.hh"

#include <sstream>

#include "game/MilUnit.hh"
#include "game/Player.hh"
#include "StructUtils.hh"
#include "ThreeDSprite.hh"

vector<MilUnitSprite*> SpriteContainer::sprites;
map<MilUnitTemplate*, int> MilUnitGraphicsInfo::indexMap;
vector<vector<doublet> > MilUnitGraphicsInfo::allFormations;
unsigned int TransportUnitGraphicsInfo::spriteIndex = 0;

MilUnitGraphicsInfo::MilUnitGraphicsInfo (MilUnit* dat) : GBRIDGE(MilUnit)(dat), TextInfo() {}
MilUnitGraphicsInfo::~MilUnitGraphicsInfo () {}

string MilUnitGraphicsInfo::strengthString (string indent) const {
  ostringstream buffer;
  MilUnit* myUnit = GBRIDGE(MilUnit)::getGameObject();
  for (MilUnitTemplate::Iterator m = MilUnitTemplate::start(); m != MilUnitTemplate::final(); ++m) {
    int num = myUnit->getUnitTypeAmount(*m);
    if (1 > num) continue;
    buffer << indent << (*m)->getName().c_str() << ": " << num << "\n";
  }
  return buffer.str();
}

void MilUnitGraphicsInfo::describe (QTextStream& str) const {
  MilUnit* myUnit = GBRIDGE(MilUnit)::getGameObject();
  str << myUnit->getName().c_str() << ":\n"
      << "  Owner: " << myUnit->getOwner()->getDisplayName().c_str() << "\n"
      << "  Strength:" << myUnit->displayString(4).c_str() << "\n"
      << "  Priority  : " << myUnit->getPriority() << "\n"
      << "  Shock     : " << myUnit->calcStrength(myUnit->getDecayConstant(), &MilUnitElement::shock) << "\n"
      << "  Fire      : " << myUnit->calcStrength(myUnit->getDecayConstant(), &MilUnitElement::range) << "\n"
      << "  Skirmish  : " << myUnit->calcStrength(myUnit->getDecayConstant(), &MilUnitElement::tacmob) << "\n"
      << "  Supplies  : " << myUnit->display(4).c_str() << "\n";
}

struct SortHelper {
  SortHelper () : unittype(0), strength(0) {}
  void clear();
  MilUnitTemplate* unittype;
  double strength;
};

void SortHelper::clear () {
  unittype = 0;
  strength = 0;
}

void MilUnitGraphicsInfo::updateSprites (MilStrength* dat) {
  // Looking for up to nine sprites, but a minimum of one.
  // Number of sprites is given by percentage of the largest military unit in the world.
  double total = dat->getTotalStrength();
  int numSprites = (int) floor(9*total / MilStrength::greatestStrength + 0.5);

  // Until required number is reached: Strongest unit gets a sprite.
  // Then give sprites to other units in order, skipping those whose
  // strength is less than M/(N+1), where M is strongest unit's strength
  // and N is number of sprites of strongest unit.

  spriteIndices.clear();
  formation.clear();
  static vector<SortHelper*> forces;
  if (0 == forces.size()) for (int i = 0; i < 9; ++i) forces.push_back(new SortHelper());
  for (int i = 0; i < 9; ++i) forces[i]->clear();

  int types = 0;
  for (map<MilUnitTemplate*, int>::iterator m = indexMap.begin(); m != indexMap.end(); ++m) {
    if (indexMap.find((*m).first) == indexMap.end()) continue; // Disregard spriteless unit types.
    forces[types]->unittype = (*m).first;
    forces[types]->strength = dat->getUnitTypeAmount(forces[types]->unittype);
    types++;
  }
  // Greater than for descending order!
  sort(forces.begin(), forces.end(), deref<SortHelper>(member_gt(&SortHelper::strength)));

  if (0 == types) {
    // No sprites for these units - use a default.
    spriteIndices.push_back((*(indexMap.begin())).second);
    formation.push_back(doublet(0, 0));
    return;
  }

  int firstSprites = 1; // Accounts for +1 in M/(N+1).
  while ((int) spriteIndices.size() < numSprites) {
    spriteIndices.push_back(indexMap[forces[0]->unittype]);
    firstSprites++;
    for (int i = 1; i < types; ++i) {
      if (forces[i]->strength < forces[0]->strength/firstSprites) break;
      if ((int) spriteIndices.size() >= numSprites) break;
      spriteIndices.push_back(indexMap[forces[i]->unittype]);
    }
  }

  assert(spriteIndices.size() < allFormations.size());
  for (unsigned int i = 0; i < spriteIndices.size(); ++i) {
    formation.push_back(allFormations[spriteIndices.size()][i]);
  }
}

TransportUnitGraphicsInfo::TransportUnitGraphicsInfo (TransportUnit* dat) : GBRIDGE(TransportUnit)(dat) {
  Logger::logStream(DebugStartup) << "Sprite index " << spriteIndex << "\n";
  spriteIndices.push_back(spriteIndex);
  formation.push_back(doublet(0, 0));
}

TransportUnitGraphicsInfo::~TransportUnitGraphicsInfo () {}

