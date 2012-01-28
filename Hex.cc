#include "Hex.hh" 
#include "PopUnit.hh"
#include "MilUnit.hh" 
#include <string> 
#include "Player.hh" 
#include <set> 
#include <cmath> 
#include "Logger.hh"
#include <cassert>
#include <cstdio>
#include <algorithm> 

std::set<Vertex*> Vertex::allVertices; 
std::set<Line*> Line::allLines;
std::set<Hex*> Hex::allHexes; 

char stringbuffer[1000]; 
int abs(int x) {
  return (x < 0 ? -x : x); 
}

Vertex::Vertex ()
  : Mirrorable<Vertex>()
  , supplies(0)
  , groupNum(0)
  , fortLevel(0)
{
  neighbours.resize(Hex::NoVertex);
  allVertices.insert(this); 
}

Vertex::Vertex (Vertex* other)
  : Mirrorable<Vertex>(other)
  , supplies(other->supplies)
  , groupNum(other->groupNum)
  , fortLevel(other->fortLevel)
{
  neighbours.resize(Hex::NoVertex);
}

Vertex::~Vertex () {
  for (std::vector<MilUnit*>::iterator u = units.begin(); u != units.end(); ++u) {
    (*u)->destroyIfReal();
  }
  units.clear(); 
}


Hex::Hex (int x, int y, TerrainType t)
  : Mirrorable<Hex>()
  , pos(x, y)
  , popGrowth(0)
  , myType(t)
  , owner(0)
  , devastation(0)
{
  initialise();
  allHexes.insert(this);

  mirror->pos.first = x;
  mirror->pos.second = y;
  mirror->popGrowth = popGrowth;
  mirror->myType = myType;
  mirror->owner = owner;
  mirror->devastation = devastation; 
  mirror->initialise(); 
}

Hex::Hex (Hex* other)
  : Mirrorable<Hex>(other)
  , pos(0, 0) // Mirror constructor gets called before main initialise list is finished!
  , popGrowth(0)
  , myType(other->myType)
  , owner(0)
  , devastation(0)
{}

void Hex::initialise () {
  vertices.resize(NoVertex);
  neighbours.resize(None);
  lines.resize(None);
  sprintf(stringbuffer, "(%i, %i) %s", pos.first, pos.second, isMirror() ? "(M)" : "");
  setName(stringbuffer);
}

Hex::~Hex () {}

Hex::TerrainType Hex::getType (char typ) {
  switch (typ) {
  case 'o': return Ocean;
  case 'm': return Mountain;
  case 'f': return Forest;
  case 'p': return Plain;
  case 'h': return Hill;
  default: return Unknown; 
  }
}

int Hex::numMovesTo (Hex const * const dat) const {
  int xdist = pos.first - dat->pos.first;
  int ydist = pos.second - dat->pos.second;
  if (0 == xdist) return abs(ydist);
  if (0 == ydist) return abs(xdist); 
  
  int maxYchange = abs(xdist) / 2;
  if ((0 == pos.first % 2) && (ydist > 0)) maxYchange++;
  if ((1 == pos.first % 2) && (ydist < 0)) maxYchange++;

  return abs(xdist) + std::max(0, abs(ydist) - maxYchange); 
}

Hex::Direction Hex::getDirection (std::string n) {
  if (n == "North") return North;
  if (n == "South") return South;
  if (n == "NorthEast") return NorthEast;
  if (n == "NorthWest") return NorthWest;
  if (n == "SouthEast") return SouthEast;
  if (n == "SouthWest") return SouthWest;
  return None; 
}

Hex::Vertices Hex::getVertex (std::string n) {
  if (n == "UpLeft") return LeftUp;
  if (n == "UpRight") return RightUp;
  if (n == "Left") return Left;
  if (n == "Right") return Right;
  if (n == "DownLeft") return LeftDown;
  if (n == "DownRight") return RightDown; 
  return NoVertex; 
}

std::string Hex::getDirectionName (Direction dat) {
  switch (dat) {
  case NorthWest: return std::string("NorthWest");
  case NorthEast: return std::string("NorthEast");
  case North: return std::string("North");
  case South: return std::string("South");
  case SouthWest: return std::string("SouthWest");
  case SouthEast: return std::string("SouthEast");
  case None:
  default:
    return std::string("None");
  }
}

std::string Hex::getVertexName (Vertices dat) {
  switch (dat) {
  case LeftUp: return std::string("UpLeft");
  case RightUp: return std::string("UpRight");
  case Left: return std::string("Left");
  case Right: return std::string("Right");
  case LeftDown: return std::string("DownLeft");
  case RightDown: return std::string("DownRight");
  case None:
  default:
    return std::string("None");
  }
}

std::pair<int, int> Hex::getPos (Direction dat) const {
  std::pair<int, int> ret = getPos(); 
  switch (dat) {
  case None:
  default:
    return ret;
  case NorthWest:
    if (0 == ret.first % 2) ret.second--; 
    ret.first--;
    break; 
  case North:
    ret.second--;
    break;
  case NorthEast:
    if (0 == ret.first % 2) ret.second--; 
    ret.first++;
    break; 
  case SouthWest:
    if (1 == ret.first % 2) ret.second++; 
    ret.first--; 
    break;
  case South:
    ret.second++;
    break; 
  case SouthEast:
    if (1 == ret.first % 2) ret.second++; 
    ret.first++; 
    break;
  }
  return ret;
}

void Hex::setNeighbour (Direction d, Hex* dat) {
  if (!dat) return; 
  neighbours[d] = dat;
  if (real == this) mirror->setNeighbour(d, dat->getMirror()); 
}

void Vertex::buildRoad (Vertex* target) {
  if (0 == roads[target]) roads[target] = 1;
  else roads[target]++;

  target->roads[this] = roads[target]; 
}

void Hex::createVertices () {
  // LeftUp vertex 
  if ((neighbours[North]) && (neighbours[North]->vertices[LeftDown])) vertices[LeftUp] = neighbours[North]->vertices[LeftDown];
  else if ((neighbours[NorthWest]) && (neighbours[NorthWest]->vertices[Right])) vertices[LeftUp] = neighbours[NorthWest]->vertices[Right];

  // Left
  if ((neighbours[NorthWest]) && (neighbours[NorthWest]->vertices[RightDown])) vertices[Left] = neighbours[NorthWest]->vertices[RightDown];
  else if ((neighbours[SouthWest]) && (neighbours[SouthWest]->vertices[RightUp])) vertices[Left] = neighbours[SouthWest]->vertices[RightUp];

  // LeftDown
  if ((neighbours[SouthWest]) && (neighbours[SouthWest]->vertices[Right])) vertices[LeftDown] = neighbours[SouthWest]->vertices[Right];
  else if ((neighbours[South]) && (neighbours[South]->vertices[LeftUp])) vertices[LeftDown] = neighbours[South]->vertices[LeftUp];

  // RightDown
  if ((neighbours[South]) && (neighbours[South]->vertices[RightUp])) vertices[RightDown] = neighbours[South]->vertices[RightUp];
  else if ((neighbours[SouthEast]) && (neighbours[SouthEast]->vertices[Left])) vertices[RightDown] = neighbours[SouthEast]->vertices[Left];

  // Right
  if ((neighbours[NorthEast]) && (neighbours[NorthEast]->vertices[LeftDown])) vertices[Right] = neighbours[NorthEast]->vertices[LeftDown];
  else if ((neighbours[SouthEast]) && (neighbours[SouthEast]->vertices[LeftUp])) vertices[Right] = neighbours[SouthEast]->vertices[LeftUp];

  // RightUp 
  if ((neighbours[North]) && (neighbours[North]->vertices[RightDown])) vertices[RightUp] = neighbours[North]->vertices[RightDown];
  else if ((neighbours[NorthEast]) && (neighbours[NorthEast]->vertices[Left])) vertices[RightUp] = neighbours[NorthEast]->vertices[Left];

  for (int i = LeftUp; i < NoVertex; ++i) {
    if (!vertices[i]) vertices[i] = new Vertex();
    vertices[i]->hexes.push_back(this); 
  }

  vertices[LeftUp]->neighbours[Right] = vertices[RightUp]; vertices[LeftUp]->mirror->neighbours[Right] = vertices[RightUp]->mirror; 
  vertices[RightUp]->neighbours[Left] = vertices[LeftUp]; vertices[RightUp]->mirror->neighbours[Left] = vertices[LeftUp]->mirror;
  
  vertices[RightUp]->neighbours[RightDown] = vertices[Right]; vertices[RightUp]->mirror->neighbours[RightDown] = vertices[Right]->mirror;
  vertices[Right]->neighbours[LeftUp] = vertices[RightUp]; vertices[Right]->mirror->neighbours[LeftUp] = vertices[RightUp]->mirror;
  
  vertices[Right]->neighbours[LeftDown] = vertices[RightDown]; vertices[Right]->mirror->neighbours[LeftDown] = vertices[RightDown]->mirror;
  vertices[RightDown]->neighbours[RightUp] = vertices[Right]; vertices[RightDown]->mirror->neighbours[RightUp] = vertices[Right]->mirror;
  
  vertices[RightDown]->neighbours[Left] = vertices[LeftDown]; vertices[RightDown]->mirror->neighbours[Left] = vertices[LeftDown]->mirror;
  vertices[LeftDown]->neighbours[Right] = vertices[RightDown]; vertices[LeftDown]->mirror->neighbours[Right] = vertices[RightDown]->mirror;
  
  vertices[LeftDown]->neighbours[LeftUp] = vertices[Left]; vertices[LeftDown]->mirror->neighbours[LeftUp] = vertices[Left]->mirror;
  vertices[Left]->neighbours[RightDown] = vertices[LeftDown]; vertices[Left]->mirror->neighbours[RightDown] = vertices[LeftDown]->mirror;
  
  vertices[Left]->neighbours[RightUp] = vertices[LeftUp]; vertices[Left]->mirror->neighbours[RightUp] = vertices[LeftUp]->mirror;
  vertices[LeftUp]->neighbours[LeftDown] = vertices[Left]; vertices[LeftUp]->mirror->neighbours[LeftDown] = vertices[Left]->mirror;
  
  for (int i = 0; i < NoVertex; ++i) {
    if (vertices[i]->getName() != "") continue;
    sprintf(stringbuffer, "[%i, %i, %s]", pos.first, pos.second, getVertexName(convertToVertex(i)).c_str());
    vertices[i]->setName(stringbuffer);
    sprintf(stringbuffer, "[%i, %i, %s (M)]", pos.first, pos.second, getVertexName(convertToVertex(i)).c_str());
    vertices[i]->getMirror()->setName(stringbuffer);
  }

  for (int i = 0; i < NoVertex; ++i) {
    mirror->vertices[i] = vertices[i]->getMirror(); 
  }
}

void Hex::setLine (Hex::Direction dir, Line* l) {
  if (l->getName() == "") {
    sprintf(stringbuffer, "{%i, %i, %s}", pos.first, pos.second, getDirectionName(dir).c_str()); 
    l->setName(stringbuffer);
    sprintf(stringbuffer, "{%i, %i, %s (M)}", pos.first, pos.second, getDirectionName(dir).c_str()); 
    l->getMirror()->setName(stringbuffer);
  }
  lines[dir] = l;
  if (real == this) mirror->setLine(dir, l->getMirror());
}

void Vertex::createLines () {
  for (NeighbourIterator vex = beginNeighbours(); vex != endNeighbours(); ++vex) {
    if (!(*vex)) continue; 
    if (getLine(*vex)) continue;
    Hex* hex1 = 0;
    Hex* hex2 = 0;
    for (std::vector<Hex*>::iterator hex = hexes.begin(); hex != hexes.end(); ++hex) {
      if ((*hex)->getDirection(*vex) == Hex::NoVertex) continue;
      if (hex1) hex2 = (*hex);
      else hex1 = (*hex); 
    }

    assert(hex1); 
    Line* line = new Line(this, (*vex), hex1, hex2);
    lines.push_back(line);
    mirror->lines.push_back(line->getMirror());
    assert(*vex);
    (*vex)->lines.push_back(line);
    if ((*vex)->mirror) (*vex)->mirror->lines.push_back(line->getMirror()); 

    switch (hex1->getDirection(this)) {
    case Hex::Right:
      if (Hex::RightUp == hex1->getDirection(*vex)) {
	hex1->setLine(Hex::NorthEast, line);
	if (hex2) hex2->setLine(Hex::SouthWest, line);
      }
      else {
	hex1->setLine(Hex::SouthEast, line);
	if (hex2) hex2->setLine(Hex::NorthWest, line);
      }
      break;
    case Hex::LeftUp:
      if (Hex::RightUp == hex1->getDirection(*vex)) {
	hex1->setLine(Hex::North, line);
	if (hex2) hex2->setLine(Hex::South, line);
      }
      else {
	hex1->setLine(Hex::NorthWest, line);
	if (hex2) hex2->setLine(Hex::SouthEast, line);
      }
      break;
    case Hex::RightUp:
      if (Hex::LeftUp == hex1->getDirection(*vex)) {
	hex1->setLine(Hex::North, line);
	if (hex2) hex2->setLine(Hex::South, line);
      }
      else {
	hex1->setLine(Hex::NorthEast, line);
	if (hex2) hex2->setLine(Hex::SouthWest, line);
      }
      break;
    case Hex::LeftDown:
      if (Hex::Left == hex1->getDirection(*vex)) {
	hex1->setLine(Hex::SouthWest, line);
	if (hex2) hex2->setLine(Hex::NorthEast, line);
      }
      else {
	hex1->setLine(Hex::South, line);
	if (hex2) hex2->setLine(Hex::North, line);
      }
      break; 
    case Hex::RightDown:
      if (Hex::LeftDown == hex1->getDirection(*vex)) {
	hex1->setLine(Hex::South, line);
	if (hex2) hex2->setLine(Hex::North, line);
      }
      else {
	hex1->setLine(Hex::SouthEast, line);
	if (hex2) hex2->setLine(Hex::NorthWest, line);
      }
      break;
    case Hex::Left:
      if (Hex::LeftUp == hex1->getDirection(*vex)) {
	hex1->setLine(Hex::NorthWest, line);
	if (hex2) hex2->setLine(Hex::SouthEast, line);
      }
      else {
	hex1->setLine(Hex::SouthWest, line);
	if (hex2) hex2->setLine(Hex::NorthEast, line);
      }
      break;
    case Hex::NoVertex:
    default:
      break; 
    }
  }
}
/*
Line* Hex::getLine (Hex::Direction dir) {
  switch (dir) {
  case None:      return 0;
  default:        return 0;
  case North:        return getVertex(LeftUp)->getLine(getVertex(RightUp));
  case NorthWest:    return getVertex(LeftUp)->getLine(getVertex(Left));
  case NorthEast:   return getVertex(Right)->getLine(getVertex(RightUp));
  case SouthWest:  return getVertex(LeftDown)->getLine(getVertex(Left));
  case SouthEast: return getVertex(RightDown)->getLine(getVertex(Right));
  case South:      return getVertex(LeftDown)->getLine(getVertex(RightDown));
  }
}
*/

Line* Vertex::getLine (Vertex* otherend) {
  for (std::vector<Line*>::iterator lin = lines.begin(); lin != lines.end(); ++lin) {
    if (!(*lin)) continue;
    if ((*lin)->getOther(this) != otherend) continue;
    return (*lin);
  }
  return 0; 
}

Hex* Line::otherHex (Hex* dat) {
  if (dat == hex1) return hex2;
  return hex1; 
}

Hex::Direction Hex::oppositeDirection (Hex::Direction dat) {
  switch (dat) {
  case NorthWest: return SouthEast;
  case NorthEast: return SouthWest;
  case North: return South;
  case South: return North;
  case SouthEast: return NorthWest;
  case SouthWest: return NorthEast; 
  case None: return None; 
  default: return None; 
  }
  return None; 
}

Hex::Vertices Hex::oppositeVertex (Hex::Vertices dat) {
  switch (dat) { 
  case LeftUp:    return RightDown;
  case RightUp:   return LeftDown;
  case Left:      return Right;
  case Right:     return Left;
  case RightDown: return LeftUp;
  case LeftDown:  return RightUp; 
  case NoVertex: return NoVertex;
  default: return NoVertex;
  }
  return NoVertex; 
}

Hex::Direction Hex::getDirection (Line const * const dat) const {
  switch (getDirection(dat->oneEnd())) {
  case LeftUp:    if (Left == getDirection(dat->twoEnd())) return NorthWest; return North; 
  case RightUp:   if (Right == getDirection(dat->twoEnd())) return NorthEast; return North; 
  case Left:      if (LeftUp == getDirection(dat->twoEnd())) return NorthWest; return SouthWest; 
  case Right:     if (RightUp == getDirection(dat->twoEnd())) return NorthEast; return SouthEast;
  case RightDown: if (LeftDown == getDirection(dat->twoEnd())) return South; return SouthEast;
  case LeftDown:  if (RightDown == getDirection(dat->twoEnd())) return South; return SouthWest; 
  case NoVertex: return None; 
  default: return None; 
  }
  return None; 
}

Vertex* Line::getOther (Vertex* vex) {
  if (vex == vex1) return vex2;
  if (vex == vex2) return vex1;
  return 0; 
}

std::pair<int, int> Hex::getNeighbourCoordinates (std::pair<int, int> pos, Direction dere) {
  switch (dere) {
  case North:
    pos.second--;
    break; 
  case South:
    pos.second++; 
    break; 

  case NorthEast:
    if (0 == pos.first % 2) pos.second--; 
    pos.first++;
    break; 
  case SouthEast:
    if (0 != pos.first % 2) pos.second++;
    pos.first++; 
    break; 

  case NorthWest:
    if (0 == pos.first % 2) pos.second--;
    pos.first--;
    break;   
  case SouthWest:
    if (0 != pos.first % 2) pos.second++;
    pos.first--;
    break;
    
  case None:
  default:
    break; 
  }
  return pos; 
}

Hex::Direction Hex::convertToDirection (int n){
  switch (n) {
  case NorthWest: return NorthWest;
  case NorthEast: return NorthEast;
  case SouthWest: return SouthWest;
  case SouthEast: return SouthEast;
  case North: return North;
  case South: return South; 
  default: return None; 
  }
}

void Hex::endOfTurn () {

}

unsigned int Hex::maxPopulation () const {
  switch (myType) {
  case Hex::Mountain: return 10;
  case Hex::Hill:     return 20;
  case Hex::Plain:    return 40;
  case Hex::Forest:   return 30;
  case Hex::Ocean:
  case Hex::Unknown:
  default:
    return 0;
  }
}

void Hex::production () {
  if (!owner) return;
  double supplies = 0;
  double prodFactor = 1;
  switch (myType) {
  case Hex::Mountain: prodFactor = 0.75; break;
  case Hex::Hill:     prodFactor = 0.85; break;
  case Hex::Plain:    prodFactor = 1.00; break;
  case Hex::Forest:   prodFactor = 0.90; break;
  case Hex::Ocean:
  case Hex::Unknown:
  default:
    prodFactor = 0; 
  }
  
  for (std::vector<PopUnit*>::iterator p = units.begin(); p != units.end(); ++p) {
    supplies += (*p)->production() * prodFactor; 
  }
  assert(!std::isnan(supplies)); 
  supplies /= vertices.size(); 
  assert(!std::isnan(supplies)); 
  for (std::vector<Vertex*>::iterator vex = vertices.begin(); vex != vertices.end(); ++vex) {
    //if (!(*vex)->canTakeSupplies(owner)) continue;
    (*vex)->addSupplies(supplies); 
  }
}

std::string Hex::toString () const {
  static char buffer[1000];
  sprintf(buffer, "(%i, %i)", pos.first, pos.second); 
  return std::string(buffer); 
}

Hex::Vertices Hex::convertToVertex (int i) {
  switch (i) {
  case LeftUp: return LeftUp;
  case RightUp: return RightUp;
  case Right: return Right;
  case RightDown: return RightDown;
  case LeftDown: return LeftDown;
  case Left: return Left;
  default:
  case NoVertex: return NoVertex;
  }
  return NoVertex; 
}

Vertex* Hex::getVertex (int i) {
  if (NoVertex == convertToVertex(i)) return 0;
  return vertices[i]; 
}

Hex::Vertices Hex::getDirection (Vertex const * const ofdis) const {
  for (int i = LeftUp; i < NoVertex; ++i) {
    if (vertices[i] == ofdis) return convertToVertex(i); 
  }
  return NoVertex; 
}

Hex::Direction Hex::getDirection (Hex const * const dat) const {
  for (int i = NorthWest; i < None; ++i) {
    if (neighbours[i] == dat) return convertToDirection(i);
  }
  return None; 
}

Hex::Vertices Vertex::getDirection (Vertex const * const ofdis) const {
  for (int i = Hex::LeftUp; i < Hex::NoVertex; ++i) {
    if (neighbours[i] == ofdis) return Hex::convertToVertex(i);
  }
  
  return Hex::NoVertex; 
}

void Hex::setOwner (Player* p) {
  owner = p;
  for (std::vector<PopUnit*>::iterator pop = units.begin(); pop != units.end(); ++pop) {
    (*pop)->setOwner(p); 
  }
}

MilUnit* Hex::mobilise () {
  double available = 0; 
  for (PopIterator p = units.begin(); p != units.end(); ++p) {
    available += (*p)->recruitsAvailable(); 
  }
  if (available < 1000) return 0;
  for (PopIterator p = units.begin(); p != units.end(); ++p) {
    (*p)->recruit(1000*(*p)->recruitsAvailable()/available);
  }
  MilUnit* ret = new MilUnit();
  ret->setOwner(getOwner()); 
  return ret;
}

void Vertex::addUnit (MilUnit* dat) {
  units.push_back(dat);
}

double Vertex::supplyNeeded () const {
  return 0; 
}

void Vertex::endOfTurn () {

}

bool Vertex::canTakeSupplies (Player* p) const {
  std::set<Player*> owners; 
  if ((0 < units.size()) && (units[0]->getOwner() == p)) return true; 

  for (std::vector<Hex*>::const_iterator hex = hexes.begin(); hex != hexes.end(); ++hex) {
    Player* curr = (*hex)->getOwner();
    if (!curr) continue;
    if (curr == p) continue;
    return false; 
  }

  return true; 
}

double Vertex::resistance (Player* p, Vertex* n) {
  for (std::vector<Vertex*>::const_iterator vex = neighbours.begin(); vex != neighbours.end(); ++vex) {
    if (n != (*vex)) continue;
    double ret = 1000 + (flow[*vex] + tempFlow[*vex])*0.001;
    bool bridge = false; 
    if (0 < n->numUnits()) {
      if (n->getUnit(0)->getOwner() != p) ret *= pow(1 + n->numUnits(), 2);
      else bridge = true; 
    }
    if (!bridge) {
      int numOwnerHexes = 0;
      for (std::vector<Hex*>::iterator hex = hexes.begin(); hex != hexes.end(); ++hex) {
	if (std::find((*vex)->hexes.begin(), (*vex)->hexes.end(), (*hex)) == (*vex)->hexes.end()) continue;
	if (p != (*hex)->getOwner()) continue;
	numOwnerHexes++; 
      }

      switch (numOwnerHexes) {
      case 2: ret /= 3; break; 
      case 0: ret *= 1e10; break;
      case 1: 
      default:
	break; 
      }
    }

    ret /= (1 + 0.5*roads[n]); 
    return ret; 
  }
  return 1e20; 
}

double Vertex::potential (Player* p) {
  assert(!std::isnan(supplies));

  bool relevant = false;
  if ((0 < numUnits()) && (getUnit(0)->getOwner() == p)) relevant = true;
  for (std::vector<Hex*>::iterator hex = hexes.begin(); hex != hexes.end(); ++hex) {
    if (!(*hex)) continue;
    if ((*hex)->getOwner() == p) relevant = true; 
  }
  if ((0 < numUnits()) && (getUnit(0)->getOwner() != p)) relevant = false;
  
  if (!relevant) return 0; 
  double ret = supplies; 
  ret -= supplyNeeded(); 
  for (std::map<Vertex*, double>::iterator f = flow.begin(); f != flow.end(); ++f) {
    ret -= (*f).second; 
  }
  
  if (std::isnan(ret)) {
    Logger::logStream(Logger::Debug) << "Potential of vertex "
				     << toString() << " "
				     << "is NaN.\n";
    
    Logger::logStream(Logger::Debug) << "Supplies: " << supplies << "\n";
    if ((0 < numUnits()) && (getUnit(0)->getOwner() == p)) Logger::logStream(Logger::Debug) << "Units: " << numUnits() << "\n";
    for (std::map<Vertex*, double>::iterator f = flow.begin(); f != flow.end(); ++f) {
      Logger::logStream(Logger::Debug) << "Flow: " << (*f).second << "\n"; 
    }
  }

  assert(!std::isnan(ret)); 
  
  return ret; 
}

void Vertex::makeFlow (Player* p) {
  double pot = potential(p);
  if (pot < 1) return; 
  double totalInvResistance = 0;
  double totalFlow = 0;
  for (std::vector<Vertex*>::iterator vex = neighbours.begin(); vex != neighbours.end(); ++vex) {
    if (0 == (*vex)) continue;
    if (groupNum != (*vex)->groupNum) continue;
    double otherPot = (*vex)->potential(p);
    if (otherPot > pot) continue; 
    totalInvResistance += 1/resistance(p, (*vex));
    assert(!std::isnan(totalInvResistance)); 
    totalFlow += (pot - otherPot); 
  }
  if (totalFlow < 1) return; 

  assert(!std::isnan(totalFlow));
  
  for (std::vector<Vertex*>::iterator vex = neighbours.begin(); vex != neighbours.end(); ++vex) {
    if (0 == (*vex)) continue;
    if (groupNum != (*vex)->groupNum) continue;
    double otherPot = (*vex)->potential(p);
    if (otherPot > pot) continue;       
    double flo = (pot - otherPot) * (totalFlow > pot ? (pot/totalFlow) : (totalFlow/pot)) / totalInvResistance;
    flo /= resistance(p, (*vex));
    assert(!std::isnan(flo)); 
    tempFlow[*vex] = flo; 
  }
}

void Vertex::reconcile () {
  for (std::vector<Vertex*>::iterator vex = neighbours.begin(); vex != neighbours.end(); ++vex) {
    if (0 == (*vex)) continue;
    flow[*vex] += tempFlow[*vex];
    (*vex)->flow[this] -= tempFlow[*vex]; 
  }
  tempFlow.clear(); 
}

double Vertex::deliverSupplies (Player* p) {
  double ret = 0; 
  for (std::vector<Vertex*>::iterator vex = neighbours.begin(); vex != neighbours.end(); ++vex) {
    if (0 == (*vex)) continue;
    if (flow[*vex] < 0) continue;
    double del = std::min(supplies, flow[*vex]);
    flow[*vex] -= del;
    double frictionLoss = 0.00025*resistance(p, (*vex))*del;
    del -= frictionLoss; 
    assert(!std::isnan(del)); 
    (*vex)->addSupplies(del);
    delivery[*vex] += del;
    ret += del; 
    if (fabs(del) > 1) Logger::logStream(Logger::Debug) << toString() << " delivered "
							<< del << " to "
							<< (*vex)->toString()
							<< " with friction loss "
							<< frictionLoss
							<< "\n"; 
  }
  return ret; 
}

void Vertex::seedGroup (Player* p) {
  std::vector<Vertex*> recurseOnThese;
  for (std::vector<Vertex*>::iterator vex = neighbours.begin(); vex != neighbours.end(); ++vex) {
    if (0 == (*vex)) continue;
    double res = resistance(p, (*vex));
    if (res > 5e4) continue;
    if ((*vex)->groupNum == groupNum) continue; 
    assert((*vex)->groupNum == -1); 
    (*vex)->groupNum = groupNum;
    recurseOnThese.push_back(*vex); 
  }

  for (std::vector<Vertex*>::iterator vex = recurseOnThese.begin(); vex != recurseOnThese.end(); ++vex) {
    (*vex)->seedGroup(p); 
  }
}

void Vertex::clearForLogistics () {
  for (Iterator vex = allVertices.begin(); vex != allVertices.end(); ++vex) {
    (*vex)->delivery.clear();
    (*vex)->flow.clear();
    (*vex)->tempFlow.clear();    
  }
}

void Vertex::logistics (Player* p) {
  Logger::logStream(Logger::Debug) << "Entering logistics for "
				   << p->getName() 
				   << "\n";
  for (Iterator vex = allVertices.begin(); vex != allVertices.end(); ++vex) {
    (*vex)->flow.clear();
    (*vex)->tempFlow.clear();    
    (*vex)->groupNum = -1; 
  }

  int numGroups = 0; 
  for (Iterator vex = allVertices.begin(); vex != allVertices.end(); ++vex) {
    if (-1 != (*vex)->groupNum) continue;
    double pot = (*vex)->potential(p);
    if (fabs(pot) < 5) continue;
    (*vex)->groupNum = numGroups++;
    (*vex)->seedGroup(p); 
  }
 
  static const double tolerance = 100;
  for (int i = 0; i < numGroups; ++i) {
    double error = 200;
    int loopCount = 0;

    while (error > tolerance) {
      if (loopCount++ > 50) {
	Logger::logStream(Logger::Debug) << "Force-breaking flow loop\n";
	break; 
      }
      
      double maxPot = -1e20;
      double minPot = 1e20;  
      for (Iterator vex = allVertices.begin(); vex != allVertices.end(); ++vex) {
	if ((*vex)->groupNum != i) continue; 
	double pot = (*vex)->potential(p);
	Logger::logStream(Logger::Debug) << (*vex)->toString() << " potential is " << pot << " " << i << "\n";
	if (pot > maxPot) maxPot = pot;
	if (pot < minPot) minPot = pot; 
      }
      error = (maxPot - minPot); 
      Logger::logStream(Logger::Debug) << "Error is " << error << " " << minPot << " " << maxPot << "\n";
      assert(!std::isnan(error));
      
      for (Iterator vex = allVertices.begin(); vex != allVertices.end(); ++vex) {
	if ((*vex)->groupNum != i) continue; 
	(*vex)->makeFlow(p); 
      }
      
      for (Iterator vex = allVertices.begin(); vex != allVertices.end(); ++vex) {
	if ((*vex)->groupNum != i) continue; 
	(*vex)->reconcile(); 
      }
    }

    for (Iterator vex = allVertices.begin(); vex != allVertices.end(); ++vex) {
      if ((*vex)->groupNum != i) continue; 
      for (std::map<Vertex*, double>::iterator flo = (*vex)->flow.begin(); flo != (*vex)->flow.end(); ++flo) {
	if ((*flo).second < 0) continue;
	double loss = (*vex)->resistance(p, (*flo).first);
	loss *= pow((*flo).second, 2);
	loss *= 1e-11;
	if (loss > 0.5*(*flo).second) loss = 0.5*(*flo).second;
	(*vex)->flow[(*flo).first] = (*flo).second - loss; 
      }
    }
    
    error = 1000;
    loopCount = 0; 
    while (error > 10) {
      error = 0;
      assert(loopCount++ < 100); 
      for (Iterator vex = allVertices.begin(); vex != allVertices.end(); ++vex) {
	if ((*vex)->groupNum != i) continue; 
	error += (*vex)->deliverSupplies(p); 
      }
    }
  }
}

QString Vertex::toString () {
  for (std::vector<Hex*>::iterator hex = hexes.begin(); hex != hexes.end(); ++hex) {
    if (!(*hex)) continue;
    QString ret = QString("Vertex %1 of hex (%2, %3)").arg(Hex::getVertexName((*hex)->getDirection(this)).c_str()).arg((*hex)->getPos().first).arg((*hex)->getPos().second);
    return ret; 
  }
  return QString("Floating vertex"); 
}

void Vertex::addSupplies (double s) {
  assert(!std::isnan(s));
  supplies += s;
  assert(!std::isnan(supplies));
}

double Vertex::getDefenseModifier () const {
  double ret = 1 + 2*fortLevel;
  for (std::vector<Hex*>::const_iterator hex = hexes.begin(); hex != hexes.end(); ++hex) {
    switch ((*hex)->getType()) {
    case Hex::Mountain: ret *= 1.10; break;
    case Hex::Hill:     ret *= 1.05; break;
    case Hex::Plain:    ret *= 1.00; break;
    case Hex::Forest:   ret *= 1.02; break;
    case Hex::Ocean:
    case Hex::Unknown:
    default:
      return 1;
    }
  }
  return ret;
}

void Vertex::buildCastle () {
  fortLevel++; 
}

bool Vertex::isLand () const {
  for (std::vector<Hex*>::const_iterator hex = hexes.begin(); hex != hexes.end(); ++hex) {
    if (Hex::Ocean == (*hex)->getType()) continue;
    if (Hex::Unknown == (*hex)->getType()) continue;
    return true; 
  }
  return false; 
}

Line::Line (Vertex* one, Vertex* two, Hex* hone, Hex* thwo)
  : Mirrorable<Line>()
  , vex1(one)
  , vex2(two)
  , hex1(hone)
  , hex2(thwo)
  , castle(0)
{
  if (mirror) {
    if (vex1) mirror->vex1 = vex1->getMirror();
    if (vex2) mirror->vex2 = vex2->getMirror();
    if (hex1) mirror->hex1 = hex1->getMirror();
    if (hex2) mirror->hex2 = hex2->getMirror();
  }
  allLines.insert(this); 
}

Line::Line (Line* other)
  : Mirrorable<Line>(other)
  , vex1(0)
  , vex2(0)
  , hex1(0)
  , hex2(0)
  , castle(0)
{}
    
Line::~Line () {
  if (castle) castle->destroyIfReal();
}

void Vertex::forceRetreat (Castle*& c, Vertex*& v) {
  MilUnit* unit = removeUnit();
  c = NULL;
  v = NULL;
  if (!unit) return; 
  unit->weaken(); 
  for (std::vector<Line*>::iterator lin = lines.begin(); lin != lines.end(); ++lin) {
    if (!(*lin)->getCastle()) continue;
    if ((*lin)->getCastle()->getOwner() != unit->getOwner()) continue;
    if ((*lin)->getCastle()->numGarrison() >= Castle::maxGarrison) continue;
    (*lin)->getCastle()->addGarrison(unit);
    c = (*lin)->getCastle(); 
    return; 
  }

  for (NeighbourIterator vex = beginNeighbours(); vex != endNeighbours(); ++vex) {
    if (!(*vex)) continue;
    if (0 < (*vex)->numUnits()) continue;
    Line* lin = getLine(*vex);
    if ((lin->getCastle()) && (lin->getCastle()->getOwner() != unit->getOwner())) continue;
    (*vex)->addUnit(unit);
    v = (*vex);
    return; 
  }
}

void Line::addCastle (Castle* dat) {
  
  
  castle = dat;
}

void Line::setMirrorState () {
  assert(mirror);
  if (castle) {
    castle->setMirrorState();
    mirror->castle = castle->getMirror();
  }
  else {
    if (mirror->castle) delete mirror->castle->getReal(); 
    mirror->castle = 0; 
  }
}

void Vertex::setMirrorState () {
  mirror->units.clear();
  if (0 < numUnits()) {
    units[0]->setMirrorState();
    mirror->units.push_back(units[0]->getMirror());
  }
}

void Hex::setMirrorState () {
  mirror->owner = owner;
  mirror->devastation = devastation; 
}

void Hex::clear () {
  for (Iterator h = begin(); h != end(); ++h) {
    (*h)->destroyIfReal();
  }
  allHexes.clear(); 
}

void Vertex::clear () {
  for (Iterator h = begin(); h != end(); ++h) {
    (*h)->destroyIfReal();
  }
  allVertices.clear(); 
}

void Line::clear () {
  for (Iterator h = begin(); h != end(); ++h) {
    (*h)->destroyIfReal();
  }
  allLines.clear(); 
}
