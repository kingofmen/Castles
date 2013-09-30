#include "GraphicsInfo.hh"
#include <cmath>
#include <sstream> 
#include "UtilityFunctions.hh" 
#include "MilUnit.hh" 
#include "Player.hh"
#include "Building.hh" 
#include "Hex.hh" 
#include "ThreeDSprite.hh" 
#include "StructUtils.hh"

double LineGraphicsInfo::maxFlow = 1;
double LineGraphicsInfo::maxLoss = 1; 
vector<HexGraphicsInfo*> HexGraphicsInfo::allHexGraphics;
vector<VertexGraphicsInfo*> VertexGraphicsInfo::allVertexGraphics;
vector<LineGraphicsInfo*> LineGraphicsInfo::allLineGraphics;
vector<ZoneGraphicsInfo*> ZoneGraphicsInfo::allZoneGraphics; 
double* GraphicsInfo::heightMap = 0; 
vector<int> FarmGraphicsInfo::textureIndices; 
vector<FarmGraphicsInfo*> FarmGraphicsInfo::allFarmInfos; 
vector<ThreeDSprite*> MilUnitGraphicsInfo::sprites;
map<MilUnitTemplate*, int> MilUnitGraphicsInfo::indexMap; 

double area (GraphicsInfo::FieldShape const& field);
bool overlaps (GraphicsInfo::FieldShape const& field1, GraphicsInfo::FieldShape const& field2); 

const double GraphicsInfo::xIncrement = 1.0; // Distance from center of hex to Right vertex. 
const double GraphicsInfo::yIncrement = sqrt(pow(xIncrement, 2) - pow(0.5*xIncrement, 2)); // Vertical distance from center to Up vertices. 
const double GraphicsInfo::xSeparation = 0.8; // Horizontal distance from Right vertex of (0, 0) to UpLeft vertex of (1, 0). 
const double GraphicsInfo::ySeparation = xSeparation/sqrt(3); // Vertical distance from Right to UpLeft as above. Half the width of a Line.
const double GraphicsInfo::zSeparation = -0.003; // Height of a single step.
const double GraphicsInfo::zOffset     = -0.003; // Offset to get lines a little above terrain texture. 

GraphicsInfo::~GraphicsInfo () {}
HexGraphicsInfo::~HexGraphicsInfo () {}
LineGraphicsInfo::~LineGraphicsInfo () {}
VertexGraphicsInfo::~VertexGraphicsInfo () {}

int GraphicsInfo::zoneSide = 4;

ZoneGraphicsInfo::ZoneGraphicsInfo () 
  : GraphicsInfo()
  , minX(10000000)
  , minY(10000000)
  , maxX(0)
  , maxY(0)  
  , width(0)
  , height(0)

{
  allZoneGraphics.push_back(this);
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
  for (vector<ZoneGraphicsInfo*>::iterator zone = allZoneGraphics.begin(); zone != allZoneGraphics.end(); ++zone) {
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

int GraphicsInfo::getHeight (int x, int y) {
  return heightMap[y*(2 + 3*zoneSide) + x]; 
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

GraphicsInfo::GraphicsInfo () 
{}

HexGraphicsInfo::HexGraphicsInfo  (Hex* h) 
  : GraphicsInfo()
  , terrain(h->getType())
  , myHex(h)
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

  ZoneGraphicsInfo* zoneInfo = ZoneGraphicsInfo::getZoneInfo(0);
  zoneInfo->addHex(this);
  allHexGraphics.push_back(this);
}

void HexGraphicsInfo::describe (QTextStream& str) const {
  str << "Hex " << myHex->getName().c_str() << "\n"
      << "  Owner      : " << (myHex->getOwner() ? myHex->getOwner()->getDisplayName().c_str() : "None") << "\n";
  Farmland* farm = myHex->getFarm();
  if (farm) {
    //str << "Devastation   : " << myHex->getDevastation() << "\n"
    str << "  Population  : " << farm->getTotalPopulation() << "\n"
	<< "  Labour      : " << farm->production() << "\n"
	<< "  Consumption : " << farm->consumption() << "\n"
	<< "  Supplies    : " << farm->getAvailableSupplies() << "\n"
	<< "  Field status:\n"
	<< "    Cleared : " << farm->getFieldStatus(Farmland::Clear) << "\n"
	<< "    Ploughed: " << farm->getFieldStatus(Farmland::Ready) << "\n"
	<< "    Sowed   : " << farm->getFieldStatus(Farmland::Sowed) << "\n"
	<< "    Sparse  : " << farm->getFieldStatus(Farmland::Ripe1) << "\n"
	<< "    Ripe    : " << farm->getFieldStatus(Farmland::Ripe2) << "\n"
	<< "    Abundant: " << farm->getFieldStatus(Farmland::Ripe3) << "\n"
	<< "    Fallow  : " << farm->getFieldStatus(Farmland::Ended) << "\n"
	<< "  Militia:";
    int totalStrength = 0;
    for (MilUnitTemplate::Iterator m = MilUnitTemplate::begin(); m != MilUnitTemplate::end(); ++m) {
      int curr = farm->getMilitiaStrength(*m);
      if (0 < curr) str << "\n    " << (*m)->name.c_str() << ": " << curr;
      totalStrength += curr;
    }
    if (0 == totalStrength) str << " None\n";
    else str << "\n    Drill level: " << farm->getMilitiaDrill() << "\n";
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
  ZoneGraphicsInfo* zone = ZoneGraphicsInfo::getZoneInfo(0); 
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
  pasture.push_back(zer);
  triplet tmp = zer + direction/3.0;
  direction = (one - zer);
  pasture.push_back(zer + direction*(5.0/6));  
  pasture.push_back(tmp + direction*(5.0/6));
  pasture.push_back(tmp);
  
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

  /*
    // Old random code
  int attempts = 0;
  while (patchArea() < 1) {
    FieldShape square;
    square.push_back(triplet(-0.5, -0.5));
    square.push_back(triplet(-0.5,  0.5));
    square.push_back(triplet( 0.5,  0.5));
    square.push_back(triplet( 0.5, -0.5));

    double degree = rand();
    degree /= RAND_MAX;
    degree *= 360;

    double move = rand();
    move /= RAND_MAX;
    move *= 0.60;
    move += 0.10;
    triplet center(move);
    center.rotatexy(degree);
    center += position; 

    degree = rand();
    degree /= RAND_MAX;
    degree *= 360;
    
    FieldShape testField;    
    for (pit s = square.begin(); s != square.end(); ++s) {
      move = rand();
      move /= RAND_MAX;
      move -= 0.5;
      move *= 0.1;
      (*s).x() += move;
      move = rand();
      move /= RAND_MAX;
      move -= 0.5;
      move *= 0.1;
      (*s).y() += move;
      (*s) *= 0.10;
      (*s) += center; 
      (*s).rotatexy(degree, center);
      testField.push_back(*s);       
    }

    bool overlap = false;
    if (++attempts < 35) {
      for (vector<FieldShape>::iterator field = biggerPatches.begin(); field != biggerPatches.end(); ++field) {
	if (!overlaps(testField, (*field))) continue;
	overlap = true;
	break; 
      }
      
      for (vector<FieldShape>::iterator field = spritePatches.begin(); field != spritePatches.end(); ++field) {
	if (!overlaps(testField, (*field))) continue;
	overlap = true;
	break; 
      }
    }
    
    if (overlap) continue;
    for (pit pt = testField.begin(); pt != testField.end(); ++pt) {
      (*pt).z() = zone->calcHeight((*pt).x(), (*pt).y()) + zOffset; 
    }

    spritePatches.push_back(testField);
    attempts = 0;
  }
  */

  

  
  // Now trees.
  DieRoll deesix(1, 3);   
  for (vector<FieldShape>::iterator field = spritePatches.begin(); field != spritePatches.end(); ++field) {
    // How many to generate?
    int numTrees = 0;
    switch (terrain) {
    default:
    case Ocean:
    case Mountain: break;
    case Forest: numTrees += deesix.roll() - 1;
    case Hill: numTrees += deesix.roll() - 1;
    case Plain: numTrees += deesix.roll() - 1;
      break;
    }
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
  for (Iterator h = begin(); h != end(); ++h) {
    ZoneGraphicsInfo* zoneInfo = ZoneGraphicsInfo::getZoneInfo(0);

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

void LineGraphicsInfo::addCastle (HexGraphicsInfo const* supportInfo) {
  castleX  = position.x();
  castleX += 0.3*supportInfo->getPosition().x();
  castleX /= 1.3;

  castleY  = position.y();
  castleY += 0.3*supportInfo->getPosition().y();
  castleY /= 1.3;  
}

void LineGraphicsInfo::describe (QTextStream& str) const {
  str << "Line: " << myLine->getName().c_str() << "\n";
    //<< "  (" << corner1.x() << ", " << corner1.y() << ")\n"
    //  << "  (" << corner2.x() << ", " << corner2.y() << ")\n"
    //  << "  (" << corner3.x() << ", " << corner3.y() << ")\n"
    //  << "  (" << corner4.x() << ", " << corner4.y() << ")\n"
    //  << "  (" << normal.x() << ", " << normal.y() << ", " << normal.z() << ")\n";
  Castle* castle = myLine->getCastle();  
  if (castle) {
    str << "Castle: \n"
	<< "  Owner     : " << castle->getOwner()->getDisplayName().c_str() << "\n"
	<< "  Recruiting: " << castle->getRecruitType()->name.c_str() << "\n"
	<< "  Supplies  : " << castle->getAvailableSupplies() << "\n"
	<< "  ";
    MilUnit* unit = castle->getGarrison(0);
    if (unit) unit->getGraphicsInfo()->describe(str);
  }
}

void LineGraphicsInfo::getCastlePosition (double& xpos, double& ypos, double& zpos) const {
  xpos = castleX;
  ypos = castleY;
  zpos = position.z(); 
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
  , myLine(l) 
{
  
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
  
  //Logger::logStream(DebugStartup) << "Line normal " << normal.x() << " " << normal.y() << " " << normal.z() << "\n"; 

  ZoneGraphicsInfo::getZoneInfo(0)->addLine(this);  
  allLineGraphics.push_back(this);
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

void LineGraphicsInfo::traverseSupplies (double amount, double lost) {
  if (fabs(amount) < 0.001) return;
  flow += amount;
  loss += lost;
  amount = 1.0 / fabs(amount);
  loss   = 1.0 / (0.001 + loss);
  if (maxFlow > amount) maxFlow = amount;
  if (maxLoss > loss) maxLoss = loss;
}

void LineGraphicsInfo::endTurn () {
  maxFlow = 1;
  maxLoss = 1;
  for (vector<LineGraphicsInfo*>::iterator l = allLineGraphics.begin(); l != allLineGraphics.end(); ++l) {
    (*l)->flow = (*l)->loss = 0; 
  }
}

VertexGraphicsInfo::VertexGraphicsInfo (Vertex* v, HexGraphicsInfo const* hex, Vertices dir)
  : GraphicsInfo()
  , myVertex(v)
{
  corner1 = hex->getCoords(dir);
  corner2 = corner1;
  corner3 = corner1; 

  pair<int, int> hexPos = hex->getHex()->getPos(); 
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

  ZoneGraphicsInfo::getZoneInfo(0)->addVertex(this);
  allVertexGraphics.push_back(this); 
}

void VertexGraphicsInfo::describe (QTextStream& str) const {
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

PlayerGraphicsInfo::PlayerGraphicsInfo () {}
PlayerGraphicsInfo::~PlayerGraphicsInfo () {}


string MilUnitGraphicsInfo::strengthString (string indent) const {
  ostringstream buffer;
  for (MilUnitTemplate::Iterator m = MilUnitTemplate::begin(); m != MilUnitTemplate::end(); ++m) {
    int num = myUnit->getUnitTypeAmount(*m);
    if (1 > num) continue;
    buffer << indent << (*m)->name.c_str() << ": " << num << "\n";
  }
  return buffer.str(); 
}


void MilUnitGraphicsInfo::describe (QTextStream& str) const {
  str << "Unit: \n"
      << "  Owner: " << myUnit->getOwner()->getDisplayName().c_str() << "\n"
      << strengthString("  ").c_str()
      << "  Efficiency: " << myUnit->getSupplyRatio() << "\n"
      << "  Priority  : " << myUnit->getPriority() << "\n"
      << "  Shock     : " << myUnit->calcStrength(myUnit->getDecayConstant(), &MilUnitElement::shock) << "\n"
      << "  Fire      : " << myUnit->calcStrength(myUnit->getDecayConstant(), &MilUnitElement::range) << "\n"
      << "  Skirmish  : " << myUnit->calcStrength(myUnit->getDecayConstant(), &MilUnitElement::tacmob) << "\n";  
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
    return;
  }

  //Logger::logStream(DebugStartup) << "\n\nNew sprite list.\n"; 
  int firstSprites = 1; // Accounts for +1 in M/(N+1). 
  while ((int) spriteIndices.size() < numSprites) {
    spriteIndices.push_back(indexMap[forces[0]->unittype]);
    firstSprites++;
    //Logger::logStream(DebugStartup) << "Add sprite " << forces[0]->unittype->name << " from strength " << forces[0]->strength << ".\n";
    for (int i = 1; i < types; ++i) {
      if (forces[i]->strength < forces[0]->strength/firstSprites) break; 
      spriteIndices.push_back(indexMap[forces[i]->unittype]);
      //Logger::logStream(DebugStartup) << "Add sprite " << forces[i]->unittype->name << " from strength " << forces[i]->strength << ".\n";
    }
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

FarmGraphicsInfo::FarmGraphicsInfo (Farmland* f)
  : myFarm(f)
{
  allFarmInfos.push_back(this); 
}

FarmGraphicsInfo::FieldInfo::FieldInfo (FieldShape f) 
  : shape(f)
  , area(-1)
  , status(Farmland::Clear)
{}

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
  // Reverse order of generation
  village = hex->getPatch(true);  
  pasture = hex->getPatch(true);  
  exercis = hex->getPatch(true);

  double currentArea = 0;
  while ((currentArea = fieldArea()) < myFarm->totalFields()) {
    FieldShape testField = hex->getPatch(); 
    fields.push_back(FieldInfo(testField));
  }
}

void FarmGraphicsInfo::updateFieldStatus () {
  for (Iterator info = begin(); info != end(); ++info) {
    double totalFieldArea = 1.0 / (*info)->myFarm->totalFields();
    double totalGraphArea = 1.0 / (*info)->fieldArea();
    fit currentField = (*info)->start(); 
    for (int s = Farmland::Clear; s < Farmland::NumStatus; ++s) {
      double percentage = (*info)->myFarm->getFieldStatus(s);
      percentage *= totalFieldArea;
      double assigned = 0.005; // Ignore less than half a percent.
      while (assigned < percentage) {
	assigned += (*currentField).area * totalGraphArea;
	(*currentField).status = s;
	++currentField;
	if (currentField == (*info)->final()) break;
      }
      if (currentField == (*info)->final()) break;      
    }
  }

  
}
