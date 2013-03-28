#include "GraphicsInfo.hh"
#include <cmath>
#include <sstream> 
#include "UtilityFunctions.hh" 
#include "MilUnit.hh" 
#include "Player.hh"
#include "Building.hh" 
#include "Hex.hh" 

double LineGraphicsInfo::maxFlow = 1;
double LineGraphicsInfo::maxLoss = 1; 
vector<HexGraphicsInfo*> HexGraphicsInfo::allHexGraphics;
vector<VertexGraphicsInfo*> VertexGraphicsInfo::allVertexGraphics;
vector<LineGraphicsInfo*> LineGraphicsInfo::allLineGraphics;
int* GraphicsInfo::heightMap = 0; 


const double xIncrement = 1.0; // Distance from center of hex to Right vertex. 
const double yIncrement = sqrt(pow(xIncrement, 2) - pow(0.5*xIncrement, 2)); // Vertical distance from center to Up vertices. 
const double xSeparation = 0.8; // Horizontal distance from Right vertex of (0, 0) to UpLeft vertex of (1, 0). 
const double ySeparation = xSeparation/sqrt(3); // Vertical distance from Right to UpLeft as above. Half the width of a Line.
const double zSeparation = -0.003; // Height of a single step. 

GraphicsInfo::~GraphicsInfo () {}
HexGraphicsInfo::~HexGraphicsInfo () {}
LineGraphicsInfo::~LineGraphicsInfo () {}
VertexGraphicsInfo::~VertexGraphicsInfo () {}

int GraphicsInfo::zoneSide = 4;

struct ZoneInfo {
  double minX;
  double minY;
  double maxX;
  double maxY;  
  double invWidth;
  double invHeight;

  void addHex (HexGraphicsInfo* hex);
  void addLine (LineGraphicsInfo* lin);
  void addVertex (VertexGraphicsInfo* vex);
  void recalc (); 
};

void ZoneInfo::recalc () {
  invWidth  = 1.0 / (maxX - minX);
  invHeight = 1.0 / (maxY - minY);
}

void ZoneInfo::addHex (HexGraphicsInfo* hex) {
  for (int i = 0; i < NoVertex; ++i) {
    triplet coords = hex->getCoords((Vertices) i);
    minX = min(minX, coords.x());
    minY = min(minY, coords.y());
    maxX = max(maxX, coords.x());
    maxY = max(maxY, coords.y());
  }
  recalc(); 
}
void ZoneInfo::addLine (LineGraphicsInfo* lin) {
  for (int i = 0; i < 4; ++i) {
    triplet coords = lin->getCorner(i);
    minX = min(minX, coords.x());
    minY = min(minY, coords.y());
    maxX = max(maxX, coords.x());
    maxY = max(maxY, coords.y());
  }
  recalc(); 
}
void ZoneInfo::addVertex (VertexGraphicsInfo* vex) {
  for (int i = 0; i < 3; ++i) {
    triplet coords = vex->getCorner(i);
    minX = min(minX, coords.x());
    minY = min(minY, coords.y());
    maxX = max(maxX, coords.x());
    maxY = max(maxY, coords.y());
  }
  recalc(); 
}

vector<ZoneInfo> zoneInfos; 

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
{
  pair<int, int> hpos = h->getPos(); 
  position.x() = (1.5 * xIncrement + xSeparation) * hpos.first;
  position.y() = (yIncrement + ySeparation) * (2*hpos.second + (hpos.first >= 0 ? hpos.first%2 : -hpos.first%2));
  int hexX = hpos.first;
  int hexY = hpos.second;
  getHeightMapCoords(hexX, hexY, NoVertex);
  position.z() = zSeparation * getHeight(hexX, hexY);

  // Make room for leftmost and upmost vertices and lines
  position.x()  += xSeparation;
  position.y() += 2*ySeparation; 

  // Minus is due to difference in Qt and OpenGL coordinate systems. 
  // Also affects the signs in the y increments in the switch. 
  cornerRight = position;
  cornerRight.x() += xIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, Right); 
  cornerRight.z() = zSeparation * getHeight(hexX, hexY);

  cornerLeft = position;
  cornerLeft.x() -= xIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, Left);
  cornerLeft.z() = zSeparation * getHeight(hexX, hexY);

  cornerLeftUp = position;
  cornerLeftUp.x() -= 0.5*xIncrement;
  cornerLeftUp.y() -= yIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, LeftUp);
  cornerLeftUp.z() = zSeparation * getHeight(hexX, hexY);
  
  cornerRightUp = position;
  cornerRightUp.x() += 0.5*xIncrement;
  cornerRightUp.y() -= yIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, RightUp);
  cornerRightUp.z() = zSeparation * getHeight(hexX, hexY);

  cornerRightDown = position;
  cornerRightDown.x() += 0.5*xIncrement;
  cornerRightDown.y() += yIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, RightDown);
  cornerRightDown.z() = zSeparation * getHeight(hexX, hexY);

  cornerLeftDown = position;
  cornerLeftDown.x() -= 0.5*xIncrement;
  cornerLeftDown.y() += yIncrement;
  hexX = hpos.first; hexY = hpos.second; getHeightMapCoords(hexX, hexY, LeftDown); 
  cornerLeftDown.z() = zSeparation * getHeight(hexX, hexY);

  if (0 == zoneInfos.size()) {
    zoneInfos.push_back(ZoneInfo());
  }
  zoneInfos[0].addHex(this); 
  
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

pair<double, double> GraphicsInfo::getTexCoords (triplet gameCoords, int zone) {
  pair<double, double> ret(gameCoords.x(), gameCoords.y());
  ret.first  -= zoneInfos[zone].minX;
  ret.second -= zoneInfos[zone].minY;
  ret.first  *= zoneInfos[zone].invWidth;
  ret.second *= zoneInfos[zone].invHeight;
  return ret; 
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
  
  zoneInfos[0].addLine(this); 
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
    corner1.z() = zSeparation * getHeight(hexX+1, hexY + (0 == hexPos.first%2 ? -2 : 1)); 
    corner2.z() = zSeparation * getHeight(hexX+1, hexY + (0 == hexPos.first%2 ? -1 : 2)); 
    corner3.z() = zSeparation * getHeight(hexX, hexY);    
    break;
  case Left:
    corner1.x()  -= xSeparation;
    corner3.x()  -= xSeparation;
    corner1.y()  -= ySeparation;
    corner3.y()  += ySeparation;
    corner1.z() = zSeparation * getHeight(hexX-1, hexY + (0 == hexPos.first%2 ? -2 : 1));
    corner2.z() = zSeparation * getHeight(hexX, hexY);
    corner3.z() = zSeparation * getHeight(hexX-1, hexY + (0 == hexPos.first%2 ? -1 : 2));
    break;
    
  case RightUp:
    corner1.y()  -= 2*ySeparation;
    corner2.x()   += xSeparation; 
    corner2.y()  -= ySeparation;
    corner1.z() = zSeparation * getHeight(hexX, hexY-1);
    corner2.z() = zSeparation * getHeight(hexX+1, hexY + (0 == hexPos.first%2 ? -2 : 1));
    corner3.z() = zSeparation * getHeight(hexX, hexY); 
    break;
    
  case RightDown:
    corner2.x()   += xSeparation;
    corner2.y()  += ySeparation;
    corner3.y()  += 2*ySeparation;
    corner1.z() = zSeparation * getHeight(hexX, hexY);
    corner2.z() = zSeparation * getHeight(hexX+1, hexY + (0 == hexPos.first%2 ? -1 : 2)); 
    corner3.z() = zSeparation * getHeight(hexX, hexY+1);
    break;
    
  case LeftUp:
    corner3.x()   -= xSeparation;
    corner3.y()  -= ySeparation;
    corner1.y()  -= 2*ySeparation;
    corner1.z() = zSeparation * getHeight(hexX, hexY-1);
    corner2.z() = zSeparation * getHeight(hexX, hexY);
    corner3.z() = zSeparation * getHeight(hexX-1, hexY + (0 == hexPos.first%2 ? -2 : 1));
    break;
    
  case LeftDown:
    corner2.y()  += 2*ySeparation;
    corner3.x()  -= xSeparation;
    corner3.y()  += ySeparation;
    corner1.z() = zSeparation * getHeight(hexX, hexY);
    corner2.z() = zSeparation * getHeight(hexX, hexY+1);
    corner3.z() = zSeparation * getHeight(hexX-1, hexY + (0 == hexPos.first%2 ? -1 : 2));
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

  zoneInfos[0].addVertex(this); 
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
    int num = unit->getUnitTypeAmount(*m);
    if (1 > num) continue;
    buffer << indent << (*m)->name.c_str() << ": " << num << "\n";
  }
  return buffer.str(); 
}


void MilUnitGraphicsInfo::describe (QTextStream& str) const {
  str << "Unit: \n"
      << "  Owner: " << unit->getOwner()->getDisplayName().c_str() << "\n"
      << strengthString("  ").c_str()
      << "  Efficiency: " << unit->getSupplyRatio() << "\n"
      << "  Priority  : " << unit->getPriority() << "\n"
      << "  Shock     : " << unit->calcStrength(unit->getDecayConstant(), &MilUnitElement::shock) << "\n"
      << "  Fire      : " << unit->calcStrength(unit->getDecayConstant(), &MilUnitElement::range) << "\n"
      << "  Skirmish  : " << unit->calcStrength(unit->getDecayConstant(), &MilUnitElement::tacmob) << "\n";  
}