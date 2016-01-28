#include "GraphicsInfo.hh"
#include <cmath>
#include <sstream>
#include "BuildingGraphics.hh"
#include "UtilityFunctions.hh"
#include "MilUnit.hh"
#include "Player.hh"
#include "Building.hh"
#include "ThreeDSprite.hh"
#include "StructUtils.hh"

double* GraphicsInfo::heightMap = 0;
vector<MilUnitSprite*> SpriteContainer::sprites;
map<MilUnitTemplate*, int> MilUnitGraphicsInfo::indexMap;
vector<vector<doublet> > MilUnitGraphicsInfo::allFormations;
map<const TextInfo*, vector<DisplayEvent> > TextInfo::recentEvents;
map<const TextInfo*, vector<DisplayEvent> > TextInfo::savingEvents;
bool TextInfo::accumulate = true;

bool overlaps (GraphicsInfo::FieldShape const& field1, GraphicsInfo::FieldShape const& field2);

const double GraphicsInfo::xIncrement = 1.0; // Distance from center of hex to Right vertex.
const double GraphicsInfo::yIncrement = sqrt(pow(xIncrement, 2) - pow(0.5*xIncrement, 2)); // Vertical distance from center to Up vertices.
const double GraphicsInfo::xSeparation = 0.8; // Horizontal distance from Right vertex of (0, 0) to UpLeft vertex of (1, 0).
const double GraphicsInfo::ySeparation = xSeparation/sqrt(3); // Vertical distance from Right to UpLeft as above. Half the width of a Line.
const double GraphicsInfo::zSeparation = -0.003; // Height of a single step.
const double GraphicsInfo::zOffset     = -0.003; // Offset to get lines a little above terrain texture.

GraphicsInfo::~GraphicsInfo () {}
MilUnitGraphicsInfo::~MilUnitGraphicsInfo () {}
TextBridge::~TextBridge () {}

int GraphicsInfo::zoneSide = 4;

int GraphicsInfo::getHeight (int x, int y) {
  return heightMap ? heightMap[y*(2 + 3*zoneSide) + x] : 0;
}

void GraphicsInfo::getHeightMapCoords (int& hexX, int& hexY, Vertices dir) {
  int realX = 2 + 3*hexX;
  int realY = 3 + 3*hexY;

  switch (dir) {
  default:
  case NoVertex:
    break;
  case Right: realX++; break;
  case Left: realX--; break;
  case RightDown:
    realX++;
    realY++;
    break;
  case RightUp:
    realX++;
    realY--;
    break;
  case LeftDown:
    realX--;
    realY++;
    break;
  case LeftUp:
    realX--;
    realY--;
    break;
  }

  hexX = realX;
  hexY = realY;
}

GraphicsInfo::GraphicsInfo () {}

TextInfo::~TextInfo () {}

void TextInfo::describe (QTextStream& /*str*/) const {}

void TextInfo::addEvent (DisplayEvent de) {
  recentEvents[this].push_back(de);
  if (accumulate) savingEvents[this].push_back(de);
}

void TextInfo::clearRecentEvents () {
  // Throw out recent events except those which
  // have been 'accumulated' in savingEvents - they
  // are the ones that occurred during player actions.
  recentEvents = savingEvents;
  savingEvents.clear();
  accumulate = false;
}


PlayerGraphicsInfo::PlayerGraphicsInfo () {}
PlayerGraphicsInfo::~PlayerGraphicsInfo () {}


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
    // No sprites for these units - use a default
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

double area (GraphicsInfo::FieldShape const& field) {
  double area = 0;
  for (unsigned int i = 1; i < field.size(); ++i) {
    area += field[i-1].x() * field[i].y();
    area -= field[i-1].y() * field[i].x();
  }

  area += field.back().x() * field[0].y();
  area -= field.back().y() * field[0].x();

  return 0.5*abs(area); // Negative for clockwise polygon, but I don't care about that.
}

bool intersect (triplet const& pt1, triplet const& pt2, triplet const& pt3, triplet const& pt4) {
  // Returns true if the line from pt1 to pt2 intersects that from pt3 to pt4.

  // Quickly check that bounding boxes overlap
  if (max(pt3.x(), pt4.x()) < min(pt1.x(), pt2.x())) return false;
  if (max(pt1.x(), pt2.x()) < min(pt3.x(), pt4.x())) return false;
  if (max(pt3.y(), pt4.y()) < min(pt1.y(), pt2.y())) return false;
  if (max(pt1.y(), pt2.y()) < min(pt3.y(), pt4.y())) return false;

  // Check whether parallel
  double det = (pt1.x() - pt2.x())*(pt3.y() - pt4.y()) - (pt1.y() - pt2.y())*(pt3.x() - pt4.x());
  if (abs(det) < 1e-6) return false; // Strictly speaking zero, but close enough
  det = 1.0 / det;

  double xintersect = (pt1.x()*pt2.y() - pt1.y()*pt2.x())*(pt3.x() - pt4.x());
  xintersect       -= (pt3.x()*pt4.y() - pt3.y()*pt4.x())*(pt1.x() - pt2.x());
  xintersect       *= det;

  // Check that x is within bounding box
  if (xintersect < min(min(pt1.x(), pt2.x()), min(pt3.x(), pt4.x()))) return false;
  if (xintersect > max(max(pt1.x(), pt2.x()), max(pt3.x(), pt4.x()))) return false;

  double yintersect = (pt1.x()*pt2.y() - pt1.y()*pt2.x())*(pt3.y() - pt4.y());
  yintersect       -= (pt3.x()*pt4.y() - pt3.y()*pt4.x())*(pt1.y() - pt2.y());
  yintersect       *= det;
  if (yintersect < min(min(pt1.y(), pt2.y()), min(pt3.y(), pt4.y()))) return false;
  if (yintersect > max(max(pt1.y(), pt2.y()), max(pt3.y(), pt4.y()))) return false;

  return true;
}

bool overlaps (GraphicsInfo::FieldShape const& field1, GraphicsInfo::FieldShape const& field2) {
  // Returns true if the polygons overlap. Overlap test is to check
  // whether any lines intersect. Notice this is O(n^2), not nice.

  for (unsigned int pt1 = 1; pt1 < field1.size(); ++pt1) {
    triplet one = field1[pt1-1];
    triplet two = field1[pt1];

    for (unsigned int pt2 = 1; pt2 < field2.size(); ++pt2) {
      triplet thr = field2[pt2-1];
      triplet fou = field2[pt2];
      if (!intersect(one, two, thr, fou)) continue;
      return true;
    }

    triplet thr = field2[field2.size()-2];
    triplet fou = field2[field2.size()-1];
    if (intersect(one, two, thr, fou)) return true;
  }

  triplet one = field2[field2.size()-2];
  triplet two = field2[field2.size()-1];
  for (unsigned int pt2 = 1; pt2 < field2.size(); ++pt2) {
    triplet thr = field2[pt2-1];
    triplet fou = field2[pt2];
    if (!intersect(one, two, thr, fou)) continue;
    return true;
  }

  triplet thr = field2[field2.size()-2];
  triplet fou = field2[field2.size()-1];
  if (intersect(one, two, thr, fou)) return true;

  return false;
}

