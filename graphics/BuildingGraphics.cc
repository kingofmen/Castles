#include "BuildingGraphics.hh"

#include <vector>

#include "game/Building.hh"
#include "game/Hex.hh"

vector<int> FarmGraphicsInfo::textureIndices;
unsigned int VillageGraphicsInfo::supplySpriteIndex = 0;
int VillageGraphicsInfo::maxCows = 15;
int VillageGraphicsInfo::suppliesPerCow = 60000;
vector<doublet> VillageGraphicsInfo::cowPositions;

CastleGraphicsInfo::CastleGraphicsInfo (Castle* castle)
  : GraphicsInfo()
  , GBRIDGE(Castle)(castle, this)
{
  const LineGraphicsInfo* lgi = castle->getLocation()->getGraphicsInfo();
  const HexGraphicsInfo* hgi = castle->getSupport()->getGraphicsInfo();
  if ((!lgi) || (!hgi)) return; // Should only happen in unit tests.
  position = lgi->getPosition();
  position += hgi->getPosition() * 0.3;
  position /= 1.3;
  position.z() = lgi->getPosition().z();
  angle = lgi->getAngle();
  normal = lgi->getNormal();
}

CastleGraphicsInfo::~CastleGraphicsInfo () {}

FarmGraphicsInfo::FarmGraphicsInfo (Farmland* f)
  : GraphicsInfo()
  , TextInfo()
  , GraphicsBridge<Farmland, FarmGraphicsInfo>(f, this)
  , SpriteContainer()
  , Iterable<FarmGraphicsInfo>(this)
{}

FarmGraphicsInfo::~FarmGraphicsInfo () {}

FarmGraphicsInfo::FieldInfo::FieldInfo (FieldShape f)
  : shape(f)
  , area(-1)
  , status(*FieldStatus::start())
{}

int FarmGraphicsInfo::FieldInfo::getIndex () const {
  return textureIndices[*status];
}

double FarmGraphicsInfo::fieldArea () {
  double ret = 0;
  for (fit f = fields.begin(); f != fields.end(); ++f) {
    if (0 > (*f).area) (*f).area = area((*f).shape)*5000*1.25;
    // Multiplying numbers on the assumption that 5000 is about the maximum number of
    // fields, and there's room for about a unit of graphics
    // in a Hex. The 1.25 is a fudge factor based on visual feedback.
    // Increase it to make fields sparser, decrease for denser.
    ret += (*f).area;
  }
  return ret;
}

void FarmGraphicsInfo::generateShapes (HexGraphicsInfo* hex) {
  double currentArea = 0;
  while ((currentArea = fieldArea()) < getGameObject()->getTotalFields()) {
    FieldShape testField = hex->getPatch();
    fields.push_back(FieldInfo(testField));
  }
}

MarketGraphicsInfo::MarketGraphicsInfo (Market* market)
  : TBRIDGE(Market)(market)
{}

MarketGraphicsInfo::~MarketGraphicsInfo () {}

void VillageGraphicsInfo::generateShapes (HexGraphicsInfo* hex) {
  // Reverse order of generation
  village = hex->getPatch(true);
  pasture = hex->getPatch(true);
  exercis = hex->getPatch(true);
}


int VillageGraphicsInfo::getHouses () const {
  return 1 + (int) floor(getGameObject()->getFractionOfMaxPop() * 19 + 0.5);
}

void FarmGraphicsInfo::updateFieldStatus () {
  for (Iterator info = Iterable<FarmGraphicsInfo>::start(); info != Iterable<FarmGraphicsInfo>::final(); ++info) {
    Farmland* currFarm = (*info)->getGameObject();
    // Status of fields.
    double totalFieldArea = 1.0 / currFarm->getTotalFields();
    double totalGraphArea = 1.0 / (*info)->fieldArea();
    fit currentField = (*info)->start();
    for (FieldStatus::Iter fs = FieldStatus::start(); fs != FieldStatus::final(); ++fs) {
      double percentage = currFarm->getFieldStatus(*fs);
      percentage *= totalFieldArea;
      double assigned = 0.005; // Ignore less than half a percent.
      while (assigned < percentage) {
	assigned += (*currentField).area * totalGraphArea;
	(*currentField).status = (*fs);
	++currentField;
	if (currentField == (*info)->final()) break;
      }
      if (currentField == (*info)->final()) break;
    }
  }
}

VillageGraphicsInfo::VillageGraphicsInfo (Village* f)
  : GraphicsInfo()
  , TextInfo()
  , GBRIDGE(Village)(f, this)
  , SpriteContainer()
  , Iterable<VillageGraphicsInfo>(this)
{}

VillageGraphicsInfo::~VillageGraphicsInfo () {}

void VillageGraphicsInfo::updateVillageStatus () {
  for (Iterator info = Iterable<VillageGraphicsInfo>::start(); info != Iterable<VillageGraphicsInfo>::final(); ++info) {
    (*info)->spriteIndices.clear();
    (*info)->formation.clear();
    //int numCows = min(maxCows, (int) floor((*info)->getGameObject()->getAvailableSupplies() / suppliesPerCow));
    unsigned int numCows = 1;
    for (unsigned int i = 0; i < numCows; ++i) {
      (*info)->spriteIndices.push_back(supplySpriteIndex);
      if (i < cowPositions.size()) (*info)->formation.push_back(cowPositions[i]);
    }
  }
}

