#include "GeoGraphics.hh"

#include "BuildingGraphics.hh"
#include "Hex.hh"
#include "MilUnit.hh"
#include "Player.hh"

double LineGraphicsInfo::maxFlow = 1;
double LineGraphicsInfo::maxLoss = 1;

HexGraphicsInfo::~HexGraphicsInfo () {}
LineGraphicsInfo::~LineGraphicsInfo () {}
VertexGraphicsInfo::~VertexGraphicsInfo () {}
ZoneGraphicsInfo::~ZoneGraphicsInfo () {}

HexGraphicsInfo::HexGraphicsInfo  (Hex* h)
  : GraphicsInfo()
  , GBRIDGE(Hex)(h, this)
  , Iterable<HexGraphicsInfo>(this)
  , farmInfo(0)
{
  pair<int, int> hpos = h->getPos();
  position.x() = (1.5 * xIncrement + xSeparation) * hpos.first;
  position.y() = (yIncrement + ySeparation) * (2*hpos.second + (hpos.first >= 0 ? hpos.first%2 : -hpos.first%2));
  int hexX = hpos.first;
  int hexY = hpos.second;
  getHeightMapCoords(hexX, hexY, NoVertex);
  position.z() = zOffset + zSeparation * getHeight(hexX, hexY);

  // Make room for leftmost and upmost vertices and lines
  position.x()  += xSeparation;
  position.y() += 2*ySeparation;

  // Minus is due to difference in Qt and OpenGL coordinate systems.
  // Also affects the signs in the y increments in the switch.
  cornerRight = position;
  cornerRight.x() += xIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, Right);
  cornerRight.z() = zOffset + zSeparation * getHeight(hexX, hexY);

  cornerLeft = position;
  cornerLeft.x() -= xIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, Left);
  cornerLeft.z() = zOffset + zSeparation * getHeight(hexX, hexY);

  cornerLeftUp = position;
  cornerLeftUp.x() -= 0.5*xIncrement;
  cornerLeftUp.y() -= yIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, LeftUp);
  cornerLeftUp.z() = zOffset + zSeparation * getHeight(hexX, hexY);

  cornerRightUp = position;
  cornerRightUp.x() += 0.5*xIncrement;
  cornerRightUp.y() -= yIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, RightUp);
  cornerRightUp.z() = zOffset + zSeparation * getHeight(hexX, hexY);

  cornerRightDown = position;
  cornerRightDown.x() += 0.5*xIncrement;
  cornerRightDown.y() += yIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, RightDown);
  cornerRightDown.z() = zOffset + zSeparation * getHeight(hexX, hexY);

  cornerLeftDown = position;
  cornerLeftDown.x() -= 0.5*xIncrement;
  cornerLeftDown.y() += yIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, LeftDown);
  cornerLeftDown.z() = zOffset + zSeparation * getHeight(hexX, hexY);

  ZoneGraphicsInfo* zoneInfo = ZoneGraphicsInfo::getByIndex(0);
  zoneInfo->addHex(this);
}

void HexGraphicsInfo::describe (QTextStream& str) const {
  Hex* myHex = getGameObject();
  str << "Hex " << myHex->getName().c_str() << "\n"
      << "  Owner      : " << (myHex->getOwner() ? myHex->getOwner()->getDisplayName().c_str() : "None") << "\n";
  Farmland* farm = myHex->getFarm();
  Village* village = myHex->getVillage();
  if (village) {
    //str << "Devastation   : " << myHex->getDevastation() << "\n"
    str << "  Population  : " << village->getTotalPopulation() << "\n"
	<< "  Labour      : " << village->production() << "\n"
	<< "  Consumption : " << village->consumption() << "\n";
      //<< "  Supplies    : " << village->getAvailableSupplies() << "\n";
  }
  if (farm) {
    FieldStatus::rIter fs = FieldStatus::rstart(); // Reverse because the invocations are in reverse code order; << operator is right-to-left!
    str << "  Field status:\n"
	<< "    Cleared : " << farm->getFieldStatus(*fs++) << "\n"
	<< "    Ploughed: " << farm->getFieldStatus(*fs++) << "\n"
	<< "    Sowed   : " << farm->getFieldStatus(*fs++) << "\n"
	<< "    Sparse  : " << farm->getFieldStatus(*fs++) << "\n"
	<< "    Ripe    : " << farm->getFieldStatus(*fs++) << "\n"
	<< "    Abundant: " << farm->getFieldStatus(*fs++) << "\n"
	<< "    Fallow  : " << farm->getFieldStatus(*fs++) << "\n";
  }
  if (village) {
    str << "  Militia:";
    int totalStrength = 0;
    for (MilUnitTemplate::Iterator m = MilUnitTemplate::start(); m != MilUnitTemplate::final(); ++m) {
      int curr = village->getMilitiaStrength(*m);
      if (0 < curr) str << "\n    " << (*m)->getName().c_str() << ": " << curr;
      totalStrength += curr;
    }
    if (0 == totalStrength) str << " None\n";
    else str << "\n    Drill level: " << village->getMilitiaDrill() << "\n";
  }
  str << "Prices, contracts, bids:\n";
  Market* market = getGameObject()->getMarket();
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    str << "  " << (*tg)->getName().c_str() << " : " << market->getPrice(*tg) << ", ";
    double contract = 0;
    BOOST_FOREACH(MarketContract* mc, market->contracts) {
      if (mc->tradeGood != (*tg)) continue;
      contract += mc->amount;
    }
    str << contract << ", " << market->demand.getAmount(*tg) << "\n";
  }
}

bool HexGraphicsInfo::isInside (double x, double y) const {
  int intersections = 0;
  triplet center = getCoords(NoVertex);
  triplet radius = getCoords(LeftUp);
  if (pow(x - center.x(), 2) + pow(y - center.y(), 2) > pow(radius.x() - center.x(), 2) + pow(radius.y() - center.y(), 2)) return false;
  for (int i = RightUp; i < NoVertex; ++i) {
    triplet coords1 = getCoords((Vertices) (i-1));
    triplet coords2 = getCoords((Vertices) i);

    if (!intersect(x, y, x+10, y+10, coords1.x(), coords1.y(), coords2.x(), coords2.y())) continue;
    intersections++;
  }
  triplet coords1 = getCoords(LeftUp);
  triplet coords2 = getCoords(Left);
  if (intersect(x, y, x+10, y+10, coords1.x(), coords1.y(), coords2.x(), coords2.y())) intersections++;
  if (0 == intersections % 2) return false;
  return true;
}

void HexGraphicsInfo::generateShapes () {
  ZoneGraphicsInfo* zone = ZoneGraphicsInfo::getByIndex(0);
  Vertices villageCorner = convertToVertex(rand());

  FieldShape exercis;
  FieldShape pasture;
  FieldShape village;

  triplet zer = getCoords(villageCorner-2);
  triplet one = getCoords(villageCorner-1);
  triplet two = getCoords(villageCorner);
  triplet thr = getCoords(villageCorner+1);
  triplet fou = getCoords(villageCorner+2);
  triplet fiv = getCoords(villageCorner+3);
  triplet direction = (fou - thr);


  exercis.push_back(thr);
  exercis.push_back(thr + direction/3.0);
  direction = (two - thr);
  exercis.push_back(exercis.back() + direction*(5.0/6));
  exercis.push_back(thr + direction*(5.0/6));


  direction = (fiv-zer);
  triplet tmp = zer + direction/3.0;
  pasture.push_back(tmp);
  pasture.push_back(zer);
  direction = (one - zer);
  pasture.push_back(zer + direction*(5.0/6));
  pasture.push_back(tmp + direction*(5.0/6));

  village.push_back(two);
  direction = (fou - two);
  village.push_back(two + direction/6.0);
  village.push_back(one + direction/6.0);
  village.push_back(one);

  biggerPatches.push_back(exercis);
  biggerPatches.push_back(pasture);
  biggerPatches.push_back(village);

  triplet updex = two - thr;
  triplet upsin = one - zer;
  for (int yfield = 0; yfield < 6; ++yfield) {
    triplet sin1  = exercis[1];
    triplet sin2  = exercis[1];
    triplet dex1 = tmp;
    triplet dex2 = tmp;
    sin1 += updex * ((0.0 + yfield) / 6);
    sin2 += updex * ((1.0 + yfield) / 6);
    dex1 += upsin * ((0.0 + yfield) / 6);
    dex2 += upsin * ((1.0 + yfield) / 6);

    int divisions = (yfield > 3 ? 3 : 2);
    triplet right1 = (dex1 - sin1);
    triplet right2 = (dex2 - sin2);
    right1 /= divisions;
    right2 /= divisions;

    for (int i = 0; i < divisions; ++i) {
      FieldShape field;
      field.push_back(sin1);
      field.push_back(sin1 + right1);
      field.push_back(sin2 + right2);
      field.push_back(sin2);

      sin1 += right1;
      sin2 += right2;

      for (pit pt = field.begin(); pt != field.end(); ++pt) {
	(*pt).z() = zone->calcHeight((*pt).x(), (*pt).y()) + zOffset;
      }

      spritePatches.push_back(field);
    }
  }

  // Now trees.
  DieRoll deesix(1, 3);
  for (vector<FieldShape>::iterator field = spritePatches.begin(); field != spritePatches.end(); ++field) {
    // How many to generate?
    int numTrees = deesix.roll() - 1;
    treesPerField.push_back(numTrees);

    double minX = min(min((*field)[0].x(), (*field)[1].x()), min((*field)[2].x(), (*field)[3].x()));
    double maxX = max(max((*field)[0].x(), (*field)[1].x()), max((*field)[2].x(), (*field)[3].x()));
    double minY = min(min((*field)[0].y(), (*field)[1].y()), min((*field)[2].y(), (*field)[3].y()));
    double maxY = max(max((*field)[0].y(), (*field)[1].y()), max((*field)[2].y(), (*field)[3].y()));

    for (int i = 0; i < numTrees; ++i) {
      triplet position(1e25, 1e25, 0);

      while (!contains((*field), position)) {
	position.x() = rand(); position.x() /= RAND_MAX;
	position.x() *= (maxX - minX);
	position.x() += minX;
	
	position.y() = rand(); position.y() /= RAND_MAX;
	position.y() *= (maxY - minY);
	position.y() += minY;
      }

      position.z() = zone->calcHeight(position.x(), position.y());
      trees.push_back(position);
    }
  }
}

double HexGraphicsInfo::patchArea () const {
  double ret = 0;
  for (vector<FieldShape>::const_iterator f = spritePatches.begin(); f != spritePatches.end(); ++f) {
    ret += area(*f);
  }
  //for (vector<FieldShape>::const_iterator f = biggerPatches.begin(); f != biggerPatches.end(); ++f) {
  //ret += area(*f);
  //}
  return ret;
}


triplet HexGraphicsInfo::getCoords (Vertices dir) const {
  switch (dir) {
  case LeftUp:    return cornerLeftUp;
  case RightUp:   return cornerRightUp;
  case Right:     return cornerRight;
  case Left:      return cornerLeft;
  case RightDown: return cornerRightDown;
  case LeftDown:  return cornerLeftDown;
  default:
  case NoVertex:  return position;
  }
}

void HexGraphicsInfo::getHeights () {
  for (Iterator h = start(); h != final(); ++h) {
    ZoneGraphicsInfo* zoneInfo = ZoneGraphicsInfo::getByIndex(0);

    (*h)->position.z()         = zoneInfo->calcHeight((*h)->position.x(),        (*h)->position.y());
    (*h)->cornerRight.z()      = zoneInfo->calcHeight((*h)->cornerRight.x(),     (*h)->cornerRight.y());
    (*h)->cornerRightUp.z()    = zoneInfo->calcHeight((*h)->cornerRightUp.x(),   (*h)->cornerRightUp.y());
    (*h)->cornerLeftUp.z()     = zoneInfo->calcHeight((*h)->cornerLeftUp.x(),    (*h)->cornerLeftUp.y());
    (*h)->cornerLeft.z()       = zoneInfo->calcHeight((*h)->cornerLeft.x(),      (*h)->cornerLeft.y());
    (*h)->cornerLeftDown.z()   = zoneInfo->calcHeight((*h)->cornerLeftDown.x(),  (*h)->cornerLeftDown.y());
    (*h)->cornerRightDown.z()  = zoneInfo->calcHeight((*h)->cornerRightDown.x(), (*h)->cornerRightDown.y());
  }
}

GraphicsInfo::FieldShape HexGraphicsInfo::getPatch (bool large) {
  vector<FieldShape>* target = large ? &biggerPatches : &spritePatches;

  FieldShape ret = target->back();
  target->pop_back();
  if (!large) {
    for (int i = 0; i < treesPerField.back(); ++i) trees.pop_back();
    treesPerField.pop_back();
  }
  return ret;
}

void HexGraphicsInfo::setFarm (FarmGraphicsInfo* f) {
  if (0 == spritePatches.size()) generateShapes();
  farmInfo = f;
  farmInfo->generateShapes(this);
}

void HexGraphicsInfo::setVillage (VillageGraphicsInfo* f) {
  if (0 == biggerPatches.size()) generateShapes();
  villageInfo = f;
  villageInfo->generateShapes(this);
}

void LineGraphicsInfo::describe (QTextStream& str) const {
  Line* myLine = getGameObject();
  str << "Line: " << myLine->getName().c_str() << "\n";
  Castle* castle = myLine->getCastle();
  if (castle) {
    str << "Castle: \n"
	<< "  Owner     : " << castle->getOwner()->getDisplayName().c_str() << "\n"
	<< "  Recruiting: " << castle->getRecruitType()->getName().c_str() << "\n"
	<< "  ";
    MilUnit* unit = castle->getGarrison(0);
    if (unit) unit->getGraphicsInfo()->describe(str);
  }
}

triplet LineGraphicsInfo::getCorner (int which) const {
  switch (which) {
  case 1: return corner2;
  case 2: return corner3;
  case 3: return corner4;
  default:
  case 0: return corner1;
  }
  return corner1;
}

LineGraphicsInfo::LineGraphicsInfo (Line* l, Vertices dir)
  : GraphicsInfo()
  , TextInfo()
  , GBRIDGE(Line)(l, this)
  , Iterable<LineGraphicsInfo>(this)
{
  Line* myLine = getGameObject();
  const VertexGraphicsInfo* vex1 = myLine->oneEnd()->getGraphicsInfo();
  const VertexGraphicsInfo* vex2 = myLine->twoEnd()->getGraphicsInfo();
  //const HexGraphicsInfo* hex     = myLine->oneHex()->getGraphicsInfo();

  // Angles are reversed due to Qt/OpenGL mismatch.
  switch (dir) {
  case Right     :
    angle = 0;
    corner1 = vex2->getCorner(0);
    corner2 = vex2->getCorner(2);
    corner3 = vex1->getCorner(1);
    corner4 = vex1->getCorner(0);
    break;
  case Left      :
    angle = 180;
    corner1 = vex1->getCorner(0);
    corner2 = vex1->getCorner(2);
    corner3 = vex2->getCorner(1);
    corner4 = vex2->getCorner(0);
    break;
  case RightUp   :
    angle = -60;
    corner1 = vex2->getCorner(2);
    corner2 = vex2->getCorner(1);
    corner3 = vex1->getCorner(1);
    corner4 = vex1->getCorner(0);
    break;
  case LeftUp    :
    angle = -120;
    corner1 = vex2->getCorner(2);
    corner2 = vex2->getCorner(1);
    corner3 = vex1->getCorner(0);
    corner4 = vex1->getCorner(2);
    break;
  case RightDown :
    angle = 60;
    corner1 = vex1->getCorner(2);
    corner2 = vex1->getCorner(1);
    corner3 = vex2->getCorner(0);
    corner4 = vex2->getCorner(2);
    break;
  default:
  case LeftDown  :
    angle = 120;
    corner1 = vex1->getCorner(2);
    corner2 = vex1->getCorner(1);
    corner3 = vex2->getCorner(1);
    corner4 = vex2->getCorner(0);
    break;
  }

  position  = corner1;
  position += corner2;
  position += corner3;
  position += corner4;
  position *= 0.25;

  // Normal is vector 1-to-4 cross vector 1-to-2, this gives upwards normal.
  triplet vec1 = corner4;
  vec1 -= corner1;
  triplet vec2 = corner2;
  vec2 -= corner1;
  normal = vec1.cross(vec2);
  normal.normalise();

  ZoneGraphicsInfo::getByIndex(0)->addLine(this);
}

bool LineGraphicsInfo::isInside (double x, double y) const {
  if (x < min(min(corner1.x(), corner2.x()), min(corner3.x(), corner4.x()))) return false;
  if (x > max(max(corner1.x(), corner2.x()), max(corner3.x(), corner4.x()))) return false;
  if (y < min(min(corner1.y(), corner2.y()), min(corner3.y(), corner4.y()))) return false;
  if (y > max(max(corner1.y(), corner2.y()), max(corner3.y(), corner4.y()))) return false;

  int intersections = 0;
  if (intersect(x, y, x+10, y+10, corner1.x(), corner1.y(), corner2.x(), corner2.y())) {intersections++;}
  if (intersect(x, y, x+10, y+10, corner2.x(), corner2.y(), corner3.x(), corner3.y())) {intersections++;}
  if (intersect(x, y, x+10, y+10, corner3.x(), corner3.y(), corner4.x(), corner4.y())) {intersections++;}
  if (intersect(x, y, x+10, y+10, corner4.x(), corner4.y(), corner1.x(), corner1.y())) {intersections++;}
  if (0 == intersections % 2) return false;
  return true;
}

void LineGraphicsInfo::endTurn () {
  maxFlow = 1;
  maxLoss = 1;
  for (Iterator l = start(); l != final(); ++l) {
    (*l)->flow = (*l)->loss = 0;
  }
}

VertexGraphicsInfo::VertexGraphicsInfo (Vertex* v, HexGraphicsInfo const* hex, Vertices dir)
  : GraphicsInfo()
  , TextInfo()
  , GBRIDGE(Vertex)(v, this)
  , Iterable<VertexGraphicsInfo>(this)
{
  corner1 = hex->getCoords(dir);
  corner2 = corner1;
  corner3 = corner1;

  pair<int, int> hexPos = hex->getGameObject()->getPos();
  int hexX = hexPos.first;
  int hexY = hexPos.second;
  getHeightMapCoords(hexX, hexY, dir);

  // Notice that negative y direction is north on the map, corresponding to 'Up' directions.
  // corner1 should always have the lowest y coordinate.
  switch (dir) {
  case Right:
    corner1.x()  += xSeparation;
    corner2.x()  += xSeparation;
    corner1.y()  -= ySeparation;
    corner2.y()  += ySeparation;
    // Counter-intuitive, happens because the hex grid is skewed relative to the square grid
    corner1.z() = zOffset + zSeparation * getHeight(hexX+1, hexY + (0 == hexPos.first%2 ? -2 : 1));
    corner2.z() = zOffset + zSeparation * getHeight(hexX+1, hexY + (0 == hexPos.first%2 ? -1 : 2));
    corner3.z() = zOffset + zSeparation * getHeight(hexX, hexY);
    break;
  case Left:
    corner1.x()  -= xSeparation;
    corner3.x()  -= xSeparation;
    corner1.y()  -= ySeparation;
    corner3.y()  += ySeparation;
    corner1.z() = zOffset + zSeparation * getHeight(hexX-1, hexY + (0 == hexPos.first%2 ? -2 : 1));
    corner2.z() = zOffset + zSeparation * getHeight(hexX, hexY);
    corner3.z() = zOffset + zSeparation * getHeight(hexX-1, hexY + (0 == hexPos.first%2 ? -1 : 2));
    break;

  case RightUp:
    corner1.y()  -= 2*ySeparation;
    corner2.x()   += xSeparation;
    corner2.y()  -= ySeparation;
    corner1.z() = zOffset + zSeparation * getHeight(hexX, hexY-1);
    corner2.z() = zOffset + zSeparation * getHeight(hexX+1, hexY + (0 == hexPos.first%2 ? -2 : 1));
    corner3.z() = zOffset + zSeparation * getHeight(hexX, hexY);
    break;

  case RightDown:
    corner2.x()   += xSeparation;
    corner2.y()  += ySeparation;
    corner3.y()  += 2*ySeparation;
    corner1.z() = zOffset + zSeparation * getHeight(hexX, hexY);
    corner2.z() = zOffset + zSeparation * getHeight(hexX+1, hexY + (0 == hexPos.first%2 ? -1 : 2));
    corner3.z() = zOffset + zSeparation * getHeight(hexX, hexY+1);
    break;

  case LeftUp:
    corner3.x()   -= xSeparation;
    corner3.y()  -= ySeparation;
    corner1.y()  -= 2*ySeparation;
    corner1.z() = zOffset + zSeparation * getHeight(hexX, hexY-1);
    corner2.z() = zOffset + zSeparation * getHeight(hexX, hexY);
    corner3.z() = zOffset + zSeparation * getHeight(hexX-1, hexY + (0 == hexPos.first%2 ? -2 : 1));
    break;

  case LeftDown:
    corner2.y()  += 2*ySeparation;
    corner3.x()  -= xSeparation;
    corner3.y()  += ySeparation;
    corner1.z() = zOffset + zSeparation * getHeight(hexX, hexY);
    corner2.z() = zOffset + zSeparation * getHeight(hexX, hexY+1);
    corner3.z() = zOffset + zSeparation * getHeight(hexX-1, hexY + (0 == hexPos.first%2 ? -1 : 2));
    break;

  default: break;
  }

  // Position is centroid.
  position = corner1;
  position.x() += corner2.x();
  position.y() += corner2.y();
  position.z() += corner2.z();
  position.x() += corner3.x();
  position.y() += corner3.y();
  position.z() += corner3.z();
  position.x() *= 0.333;
  position.y() *= 0.333;
  position.z() *= 0.333;

  ZoneGraphicsInfo::getByIndex(0)->addVertex(this);
}

void VertexGraphicsInfo::describe (QTextStream& str) const {
  Vertex* myVertex = getGameObject();
  str << "Vertex: " << myVertex->getName().c_str() << "\n";
  MilUnit* unit = myVertex->getUnit(0);
  if (unit) {
    unit->getGraphicsInfo()->describe(str);
  }
}

bool VertexGraphicsInfo::isInside (double x, double y) const {
  if (x < min(corner1.x(), min(corner2.x(), corner3.x()))) return false;
  if (x > max(corner1.x(), max(corner2.x(), corner3.x()))) return false;
  if (y < min(corner1.y(), min(corner2.y(), corner3.y()))) return false;
  if (y > max(corner1.y(), max(corner2.y(), corner3.y()))) return false;
  int intersections = 0;
  if (intersect(x, y, x+10, y+10, corner1.x(), corner1.y(), corner2.x(), corner2.y())) intersections++;
  if (intersect(x, y, x+10, y+10, corner1.x(), corner1.y(), corner3.x(), corner3.y())) intersections++;
  if (intersect(x, y, x+10, y+10, corner3.x(), corner3.y(), corner2.x(), corner2.y())) intersections++;
  if (0 == intersections % 2) return false;
  return true;
}

ZoneGraphicsInfo::ZoneGraphicsInfo ()
  : GraphicsInfo()
  , Iterable<ZoneGraphicsInfo>(this)
  , Numbered<ZoneGraphicsInfo>(this)
  , minX(10000000)
  , minY(10000000)
  , maxX(0)
  , maxY(0)
  , width(0)
  , height(0)
{
  heightMap = new double*[zoneSize];
  for (int i = 0; i < zoneSize; ++i) heightMap[i] = new double[zoneSize];
  recalc();
}

void ZoneGraphicsInfo::recalc () {
  width  = maxX - minX;
  height = maxY - minY;
  grid.push_back(vector<triplet>());
}

void ZoneGraphicsInfo::gridAdd (triplet coords) {
  minX = min(minX, coords.x());
  minY = min(minY, coords.y());
  maxX = max(maxX, coords.x());
  maxY = max(maxY, coords.y());
  grid.back().push_back(coords);
}

void ZoneGraphicsInfo::addHex (HexGraphicsInfo* hex) {
  for (int i = 0; i < NoVertex; ++i) {
    gridAdd(hex->getCoords((Vertices) i));
  }
  recalc();
}

void ZoneGraphicsInfo::addLine (LineGraphicsInfo* lin) {
  for (int i = 0; i < 4; ++i) {
    gridAdd(lin->getCorner(i));
  }
  recalc();
}

void ZoneGraphicsInfo::addVertex (VertexGraphicsInfo* vex) {
  for (int i = 0; i < 3; ++i) {
    gridAdd(vex->getCorner(i));
  }
  recalc();
}

int ZoneGraphicsInfo::badLine (triplet one, triplet two) {
  triplet current;
  for (int i = 1; i < 100; ++i) {
    current.x() = 0.01 * (i*two.x() + (100 - i) * one.x());
    current.y() = 0.01 * (i*two.y() + (100 - i) * one.y());
    current.z() = 0.01 * (i*two.z() + (100 - i) * one.z());

    // Greater than is correct because larger heights are negative.
    if (current.z() - zOffset*0.33 > calcHeight(current.x(), current.y())) return i;
  }

  return -1;
}

void ZoneGraphicsInfo::calcGrid () {
  // Get heights for the grid objects.
  for (Iter zone = start(); zone != final(); ++zone) {
    for (gridIt i = (*zone)->grid.begin(); i != (*zone)->grid.end(); ++i) {
      // Remove empty end object.
      if (0 == (*i).size()) {
	(*zone)->grid.pop_back();
	break;
      }

      vector<triplet> result;
      for (hexIt t = (*i).begin(); t != (*i).end(); ++t) {
	(*t).z() = zOffset + (*zone)->calcHeight((*t).x(), (*t).y());
      }

      (*i).push_back((*i).front());
      result.push_back((*i).front());

      for (unsigned int trip = 1; trip < (*i).size(); ++trip) {
	int halves = 0;
	loopStart:
	if (halves++ > 101) {
	  //Logger::logStream(DebugStartup) << "Reach failure point\n";
	  break;
	}
	triplet workingPoint = (*i)[trip];
	bool hadToHalve = false;

	int badPoint = 0;
	while (-1 < (badPoint = (*zone)->badLine(result.back(), workingPoint))) {
	  workingPoint.x() = 0.01 * ((100 - badPoint)*result.back().x() + badPoint*workingPoint.x());
	  workingPoint.y() = 0.01 * ((100 - badPoint)*result.back().y() + badPoint*workingPoint.y());
	  workingPoint.z() = zOffset + (*zone)->calcHeight(workingPoint.x(), workingPoint.y());
	  hadToHalve = true;
	}

	result.push_back(workingPoint);
	if (hadToHalve) goto loopStart;
      }

      (*i).clear();
      (*i) = result;
    }
  }
}

double ZoneGraphicsInfo::calcHeight (double x, double y) {
  // Get the height at point (x, y).
  x -= minX;
  y -= minY;
  x /= width;
  y /= height;
  x *= GraphicsInfo::zoneSize;
  y *= GraphicsInfo::zoneSize;
  int xval = min((int) floor(x), GraphicsInfo::zoneSize-1); // Deal with roundoff error at edges.
  int yval = min((int) floor(y), GraphicsInfo::zoneSize-1);
  return heightMap[xval][yval];
}
