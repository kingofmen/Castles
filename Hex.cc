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
#include "UtilityFunctions.hh" 

char stringbuffer[1000]; 
int abs(int x) {
  return (x < 0 ? -x : x); 
}

Vertex::Vertex ()
  : Mirrorable<Vertex>()
  , Named<Vertex>()
  , Iterable<Vertex>(this)
  , groupNum(0)
  , graphicsInfo(0)    
{
  neighbours.resize(NoVertex);
}

Vertex::Vertex (Vertex* other)
  : Mirrorable<Vertex>(other)
  , Named<Vertex>()
  , Iterable<Vertex>(1)
  , groupNum(other->groupNum)
  , graphicsInfo(0)    
{
  neighbours.resize(NoVertex);
}

Vertex::~Vertex () {
  for (std::vector<MilUnit*>::iterator u = units.begin(); u != units.end(); ++u) {
    (*u)->destroyIfReal();
  }
  units.clear();
}

void Hex::createHex (int x, int y, TerrainType t) {
  new Hex(x, y, t);
}

Hex::Hex (int x, int y, TerrainType t)
  : Mirrorable<Hex>()
  , Named<Hex>()
  , Iterable<Hex>(this)
  , pos(x, y)
  , myType(t)
  , owner(0)
  , graphicsInfo(0)        
  , farms(0)
  , forest(0)
  , mine(0)
  , village(0)
  , castle(0)
{
  initialise();

  mirror->pos.first = x;
  mirror->pos.second = y;
  mirror->myType = myType;
  mirror->owner = owner;
  mirror->initialise();
}

Hex::Hex (Hex* other)
  : Mirrorable<Hex>(other)
  , Named<Hex>()
  , Iterable<Hex>(1)
  , pos(0, 0) // Mirror constructor gets called before main initialise list is finished!
  , myType(other->myType)
  , owner(0)
  , graphicsInfo(0)
  , farms(0)
  , forest(0)
  , mine(0)
  , village(0)
  , castle(0)    
{}

void Hex::initialise () {
  vertices.resize(NoVertex);
  neighbours.resize(NoDirection);
  lines.resize(NoDirection);
  sprintf(stringbuffer, "(%i, %i) %s", pos.first, pos.second, isMirror() ? "(M)" : "");
  setName(stringbuffer);
}

Hex::~Hex () {
  if (farms) farms->destroyIfReal();
  if (forest) forest->destroyIfReal();
  if (mine) mine->destroyIfReal();
  if (village) village->destroyIfReal();
}

Hex* Hex::getHex (int x, int y) {
  for (Iterator i = start(); i != final(); ++i) { 
    if ((*i)->pos.first != x) continue;
    if ((*i)->pos.second != y) continue;
    return (*i); 
  }
  return 0; 
}

TerrainType Hex::getType (char typ) {
  switch (typ) {
  case 'o': return Ocean;
  case 'm': return Mountain;
  case 'f': return Wooded;
  case 'p': return Plain;
  case 'h': return Hill;
  default: return NoTerrain; 
  }
}

int Hex::recruit (Player* forhim, MilUnitTemplate const* const recruitType, MilUnit* target, Outcome out) {
  if (forhim != getOwner()) return 0;
  if (!village) return 0;
  village->increaseTradition(recruitType); 
  return village->produceRecruits(recruitType, target, out); 
}

GoodsHolder Hex::loot (double lootRatio) {
  GoodsHolder ret;
  if (village) ret += village->loot(lootRatio);
  if (farms)   ret += farms->loot(lootRatio);
  if (forest)  ret += forest->loot(lootRatio);
  if (mine)    ret += mine->loot(lootRatio);
  return ret;
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


std::pair<int, int> Hex::getPos (Direction dat) const {
  std::pair<int, int> ret = getPos(); 
  switch (dat) {
  case NoDirection:
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
    if (!vertices[i]) {
      vertices[i] = new Vertex();
    }
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
    if (vertices[i]->getName() != "ToBeNamed") continue;
    sprintf(stringbuffer, "[%i, %i, %s]", pos.first, pos.second, getVertexName(convertToVertex(i)).c_str());
    vertices[i]->setName(stringbuffer);
    sprintf(stringbuffer, "[%i, %i, %s (M)]", pos.first, pos.second, getVertexName(convertToVertex(i)).c_str());
    vertices[i]->getMirror()->setName(stringbuffer);
  }

  for (int i = 0; i < NoVertex; ++i) {
    mirror->vertices[i] = vertices[i]->getMirror(); 
  }
}

void Hex::setFarm (Farmland* f) {
  farms = f;
  farms->setMarket(this);
  if (village) {
    farms->setDefaultOwner(village);
    village->setFarm(farms);
  }
  setGraphicsFarm(f);
} 

void Hex::setForest (Forest* f) {
  forest = f;
  forest->setMarket(this);
  if (village) forest->setDefaultOwner(village);
}

void Hex::setGraphicsFarm (Farmland* f) {
  if (graphicsInfo) graphicsInfo->setFarm(new FarmGraphicsInfo(f)); 
}

void Hex::setGraphicsVillage (Village* f) {
  if (graphicsInfo) graphicsInfo->setVillage(new VillageGraphicsInfo(f)); 
}

void Hex::setMine (Mine* m) {
  mine = m;
  mine->setMarket(this);
  if (village) mine->setDefaultOwner(village);
}

void Hex::setVillage (Village* f) {
  village = f;
  registerParticipant(village);
  if (farms) {
    village->setFarm(farms);
    farms->setDefaultOwner(village); 
  }
  setGraphicsVillage(f);
} 

void Hex::setLine (Direction dir, Line* l) {
  if (l->getName() == "ToBeNamed") {
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
      if ((*hex)->getDirection(*vex) == NoVertex) continue;
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
    case Right:
      if (RightUp == hex1->getDirection(*vex)) {
	hex1->setLine(NorthEast, line);
	if (hex2) hex2->setLine(SouthWest, line);
      }
      else {
	hex1->setLine(SouthEast, line);
	if (hex2) hex2->setLine(NorthWest, line);
      }
      break;
    case LeftUp:
      if (RightUp == hex1->getDirection(*vex)) {
	hex1->setLine(North, line);
	if (hex2) hex2->setLine(South, line);
      }
      else {
	hex1->setLine(NorthWest, line);
	if (hex2) hex2->setLine(SouthEast, line);
      }
      break;
    case RightUp:
      if (LeftUp == hex1->getDirection(*vex)) {
	hex1->setLine(North, line);
	if (hex2) hex2->setLine(South, line);
      }
      else {
	hex1->setLine(NorthEast, line);
	if (hex2) hex2->setLine(SouthWest, line);
      }
      break;
    case LeftDown:
      if (Left == hex1->getDirection(*vex)) {
	hex1->setLine(SouthWest, line);
	if (hex2) hex2->setLine(NorthEast, line);
      }
      else {
	hex1->setLine(South, line);
	if (hex2) hex2->setLine(North, line);
      }
      break; 
    case RightDown:
      if (LeftDown == hex1->getDirection(*vex)) {
	hex1->setLine(South, line);
	if (hex2) hex2->setLine(North, line);
      }
      else {
	hex1->setLine(SouthEast, line);
	if (hex2) hex2->setLine(NorthWest, line);
      }
      break;
    case Left:
      if (LeftUp == hex1->getDirection(*vex)) {
	hex1->setLine(NorthWest, line);
	if (hex2) hex2->setLine(SouthEast, line);
      }
      else {
	hex1->setLine(SouthWest, line);
	if (hex2) hex2->setLine(NorthEast, line);
      }
      break;
    case NoVertex:
    default:
      break; 
    }
  }
}
/*
Line* Hex::getLine (Direction dir) {
  switch (dir) {
  case NoDirection:      return 0;
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

Castle* Hex::getCastle () {
  for (LineIterator lin = linBegin(); lin != linEnd(); ++lin) {
    Castle* cand = (*lin)->getCastle();
    if (!cand) continue;
    if (cand->getSupport() == this) return cand;
  }
  return 0;
}

Direction Hex::getDirection (Line const * const dat) const {
  switch (getDirection(dat->oneEnd())) {
  case LeftUp:    if (Left == getDirection(dat->twoEnd())) return NorthWest; return North; 
  case RightUp:   if (Right == getDirection(dat->twoEnd())) return NorthEast; return North; 
  case Left:      if (LeftUp == getDirection(dat->twoEnd())) return NorthWest; return SouthWest; 
  case Right:     if (RightUp == getDirection(dat->twoEnd())) return NorthEast; return SouthEast;
  case RightDown: if (LeftDown == getDirection(dat->twoEnd())) return South; return SouthEast;
  case LeftDown:  if (RightDown == getDirection(dat->twoEnd())) return South; return SouthWest; 
  case NoVertex: return NoDirection; 
  default: return NoDirection; 
  }
  return NoDirection; 
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
    
  case NoDirection:
  default:
    break; 
  }
  return pos; 
}

void Hex::endOfTurn () {
  // Farms buy labor and tools.
  holdMarket();
  // Use them to work the fields, and perhaps deliver food to owners.
  if (farms)  farms-> endOfTurn();
  if (forest) forest->endOfTurn();
  if (mine)   mine->  endOfTurn();
  // Who then consume.
  if (village) village->endOfTurn();
}

std::string Hex::toString () const {
  static char buffer[1000];
  sprintf(buffer, "(%i, %i)", pos.first, pos.second); 
  return std::string(buffer); 
}

Vertex* Hex::getVertex (int i) {
  if (NoVertex == convertToVertex(i)) return 0;
  return vertices[i]; 
}

Vertices Hex::getDirection (Vertex const * const ofdis) const {
  for (int i = LeftUp; i < NoVertex; ++i) {
    if (vertices[i] == ofdis) return convertToVertex(i); 
  }
  return NoVertex; 
}

Direction Hex::getDirection (Hex const * const dat) const {
  for (int i = NorthWest; i < NoDirection; ++i) {
    if (neighbours[i] == dat) return convertToDirection(i);
  }
  return NoDirection; 
}

Vertices Vertex::getDirection (Vertex const * const ofdis) const {
  for (int i = LeftUp; i < NoVertex; ++i) {
    if (neighbours[i] == ofdis) return convertToVertex(i);
  }
  
  return NoVertex; 
}

void Hex::setOwner (Player* p) {
  owner = p;
  for (std::vector<PopUnit*>::iterator pop = units.begin(); pop != units.end(); ++pop) {
    (*pop)->setOwner(p); 
  }
}

void Vertex::addUnit (MilUnit* dat) {
  units.push_back(dat);
  dat->setLocation(this); 
}

void Vertex::endOfTurn () {

}

void Vertex::getNeighbours (vector<Geography*>& ret) {
  for (Hex::LineIterator l = beginLines(); l != endLines(); ++l) if ((*l) && (!(*l)->closed)) ret.push_back(*l);
}

double Geography::heuristicDistance (Geography* other) const {
  return sqrt(pow(position.first - other->position.first, 2) + pow(position.second - other->position.second, 2));
}

Geography::~Geography () {} 

double Vertex::traversalCost (Player* side) const {
  return 1; 
}

double Vertex::traversalRisk (Player* side) const {
  if ((0 < units.size()) && (units[0]->getOwner() != side)) return 0.99;
  return 0.01; 
}

QString Vertex::toString () {
  for (std::vector<Hex*>::iterator hex = hexes.begin(); hex != hexes.end(); ++hex) {
    if (!(*hex)) continue;
    QString ret = QString("Vertex %1 of hex (%2, %3)").arg(getVertexName((*hex)->getDirection(this)).c_str()).arg((*hex)->getPos().first).arg((*hex)->getPos().second);
    return ret; 
  }
  return QString("Floating vertex"); 
}

bool Vertex::isLand () const {
  for (std::vector<Hex*>::const_iterator hex = hexes.begin(); hex != hexes.end(); ++hex) {
    if (Ocean == (*hex)->getType()) continue;
    if (NoTerrain == (*hex)->getType()) continue;
    return true; 
  }
  return false; 
}

// Real constructor 
Line::Line (Vertex* one, Vertex* two, Hex* hone, Hex* thwo)
  : Mirrorable<Line>()
  , Named<Line>()
  , Iterable<Line>(this)
  , vex1(one)
  , vex2(two)
  , hex1(hone)
  , hex2(thwo)
  , castle(0)
  , graphicsInfo(0)
{
  assert(vex1);
  assert(vex2);
  assert(hex1);
  mirror->vex1 = vex1->getMirror();
  mirror->vex2 = vex2->getMirror();
  mirror->hex1 = hex1->getMirror();
  if (hex2) mirror->hex2 = hex2->getMirror();
  position.first = 0.5*(vex1->position.first + vex2->position.first);
  position.second = 0.5*(vex1->position.second + vex2->position.second);
}

// Mirror constructor
Line::Line (Line* other)
  : Mirrorable<Line>(other)
  , Named<Line>()
  , Iterable<Line>(1)
  , vex1(0)
  , vex2(0)
  , hex1(0)
  , hex2(0)
  , castle(0)
  , graphicsInfo(0)    
{}
    
Line::~Line () {
  if (castle) castle->destroyIfReal();
  delete graphicsInfo; 
}

void Vertex::forceRetreat (Castle*& c, Vertex*& v) {
  MilUnit* unit = removeUnit();
  c = NULL;
  v = NULL;
  if (!unit) return; 
  for (std::vector<Line*>::iterator lin = lines.begin(); lin != lines.end(); ++lin) {
    if (!(*lin)->getCastle()) continue;
    if ((*lin)->getCastle()->getOwner() != unit->getOwner()) continue;
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

void Line::addGraphicCastle (Castle* dat) {
  if (graphicsInfo) graphicsInfo->addCastle(dat->getSupport()->getGraphicsInfo());
}

void Line::addCastle (Castle* dat) {
  addGraphicCastle(dat); 
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
  if (farms) {
    farms->setMirrorState();
    mirror->farms = farms->getMirror();
  }
  else {
    if (mirror->farms) delete mirror->farms->getReal(); 
    mirror->farms = 0; 
  }
  if (village) {
    village->setMirrorState();
    mirror->village = village->getMirror();
  }
  else {
    if (mirror->village) delete mirror->village->getReal(); 
    mirror->village = 0; 
  }
  if (forest) {
    forest->setMirrorState();
    mirror->forest = forest->getMirror();
  }
  else {
    if (mirror->forest) delete mirror->forest->getReal();
    mirror->forest = 0;
  }
  if (mine) {
    mine->setMirrorState();
    mirror->mine = mine->getMirror();
  }
  else {
    if (mirror->mine) delete mirror->mine->getReal();
    mirror->mine = 0;
  }
  if (castle) mirror->castle = castle->getMirror();
  else mirror->castle = 0;
}

void Hex::unitTests () {}

bool Hex::colonise (Line* lin, MilUnit* unit, Outcome out) {
  // Sanity checks 
  if (!unit) return false;
  if (getOwner()) return false;
  if (lin->getCastle()) return false;
  
  bool success = true;
  if (village) {
    MilUnit* defenders = village->raiseMilitia();
    if (defenders) {
      if (isReal()) {
	Logger::logStream(Logger::Game) << "Subjugation battle with dieroll " << outcomeToString(out) << ":\n"
					<< unit->getGraphicsInfo()->strengthString("  ")
					<< "versus\n"
					<< defenders->getGraphicsInfo()->strengthString("  ");
      }
      BattleResult outcome = unit->attack(defenders, out);
      success = (VictoGlory == outcome.attackerSuccess);
      village->demobMilitia();
      if (isReal()) {
	battleReport(Logger::logStream(Logger::Game), outcome);
	/*
	Logger::logStream(Logger::Game) << "Strengths: "
					<< outcome.attackerInfo.shock << " " << outcome.attackerInfo.range << " : "
					<< outcome.attackerInfo.mobRatio   << " : "
					<< outcome.defenderInfo.shock << " " << outcome.defenderInfo.range << "\n"
					<< "Efficiencies: "
					<< outcome.attackerInfo.efficiency << " "
					<< outcome.attackerInfo.fightingFraction << " "
					<< outcome.attackerInfo.decayConstant << " : "
					<< outcome.defenderInfo.efficiency << " "
					<< outcome.defenderInfo.fightingFraction << " "
					<< outcome.defenderInfo.decayConstant << "\n";
	sprintf(strbuffer, "%.1f%% shock, %.1f%% fire", 100*outcome.shockPercent, 100*outcome.rangePercent);
	Logger::logStream(Logger::Game) << "Fought with: " << strbuffer << "\n"
					<< "Casualties: " << outcome.attackerInfo.casualties << " / " << outcome.defenderInfo.casualties << "\n"
					<< "Result: " << (VictoGlory == outcome.attackerSuccess ? "VictoGlory!" : "Failure.") << "\n";
	*/
      }
    }
  }

  if (!success) return false;
  
  castle = new Castle(this, lin);
  castle->setOwner(unit->getOwner());
  setOwner(unit->getOwner()); 
  addContent(lin, castle, &Line::addCastle);
  return true;  
}

void Hex::raid (MilUnit* raiders, Outcome out) {
  if (!village) return;
  MilUnit* defenders = village->raiseMilitia();
  defenders->setExtMod(3.0);

  if (isReal()) Logger::logStream(Logger::Game) << "Raiding " << getName() << " with dieroll " << outcomeToString(out) << "\n"; 
  
  double acceptableCasualties = 0.1; // Expected casualties if a detachment is forced to fight.
  double requiredFraction = 1.0; 

  double upperFract = 0.9999;   
  raiders->setFightingFraction(upperFract);
  double currBound = raiders->calcBattleCasualties(defenders);
  if (currBound > acceptableCasualties) {
    // Can't be done acceptably.
    if (isReal()) Logger::logStream(Logger::Game) << "  Defenses too strong.\n";
    return; 
  }

  double lowerFract = 0.01;   
  raiders->setFightingFraction(lowerFract);
  currBound = raiders->calcBattleCasualties(defenders);


  if (currBound < acceptableCasualties) requiredFraction = lowerFract;
  else {
    for (int i = 0; i < 25; ++i) {
      requiredFraction = (upperFract + lowerFract)*0.5;
      raiders->setFightingFraction(requiredFraction);
      currBound = raiders->calcBattleCasualties(defenders);
      if (currBound > acceptableCasualties) lowerFract = requiredFraction;
      else upperFract = requiredFraction;

      if (upperFract - lowerFract < 0.02) break;
      if (fabs(currBound - acceptableCasualties) < 0.01) break; 
    }
  }
  
  raiders->setFightingFraction(requiredFraction);
  double baseChance = 25;
  baseChance += defenders->getScoutingModifier(raiders);
  baseChance *= 0.01; 
  
  int caught = (int) floor(baseChance * 0.5 / requiredFraction); // One-half to represent ambush forward and backward. 
  int successful  = (int) floor((1 - baseChance) / requiredFraction);

  int devastation = caught + successful;
  
  if (isReal()) Logger::logStream(Logger::Game) << "  Attacker splits into " << (successful + 2*caught) << " raiding parties.\n"; 

  int totalAttackerCasualties = 0;
  int totalDefenderCasualties = 0;
  int totalAttackerWins = 0; 

  if (isReal()) Logger::logStream(Logger::Game) << "  " << 2*caught << " skirmishes:\n"; 
  for (int i = 0; i < caught; ++i) {
    BattleResult skirmish = raiders->attack(defenders, out);
    if (VictoGlory == skirmish.attackerSuccess) {
      devastation++; // Ambush on the way in - still success if the ambush is beaten
      totalAttackerWins++;
    }

    totalAttackerCasualties += skirmish.attackerInfo.casualties;
    totalDefenderCasualties += skirmish.defenderInfo.casualties;
    
    skirmish = raiders->attack(defenders, out); // Ambush on the way back - still burn the farms
    if (VictoGlory == skirmish.attackerSuccess) totalAttackerWins++;
    totalAttackerCasualties += skirmish.attackerInfo.casualties;
    totalDefenderCasualties += skirmish.defenderInfo.casualties;
    
  }

  if (isReal()) Logger::logStream(Logger::Game) << "  Raiders win "
						<<  totalAttackerWins
						<< ", casualties are "
						<< totalAttackerCasualties << " vs "
						<< totalDefenderCasualties << "\n  Devastation: "
						<< devastation
						<< "\n"; 
  
  
  defenders->dropExtMod();
  raiders->setFightingFraction(1.0); 
  village->demobMilitia();
  farms->devastate(devastation); 
}


void Hex::clear () {
  while (0 < totalAmount()) {
    Iter h = start();
    (*h)->destroyIfReal();
  }
  Named<Hex>::clear();
}

void Vertex::clear () {
  while (0 < totalAmount()) {
    Iter h = start();
    (*h)->destroyIfReal();
  }
  Named<Vertex>::clear();
}

void Line::clear () {  
  while (0 < totalAmount()) {
    Iter h = start();
    (*h)->destroyIfReal();
  }
  Named<Line>::clear(); 
}

void Line::endOfTurn () {
  if (!castle) return;
  castle->endOfTurn(); 
}

void Line::getNeighbours (vector<Geography*>& ret) {
  if (!vex1->closed) ret.push_back(vex1);
  if (!vex2->closed) ret.push_back(vex2);
}

double Line::traversalCost (Player* side) const { 
  return 1; 
}

double Line::traversalRisk (Player* side) const {
  if ((castle) && (castle->getOwner() != side)) return 0.99;
  return 0.01; 
}


void Geography::clearGeography () {
  closed = false;
  previous = 0;
  distFromStart = 1e100;
}

double Geography::traverseSupplies (double& amount, Player* side, Geography* previous) {
  double loss = traversalCost(side);
  //Logger::logStream(DebugSupply) << "Traversed " << (int) (*g) << " at cost " << (*g)->traversalCost(*p) << "\n"; 
  double lossChance = traversalRisk(side);
  double roll = rand();
  roll /= RAND_MAX;
  if (roll < lossChance) loss += amount*0.9;
  amount -= loss;
  return loss; 
}

double Line::traverseSupplies (double& amount, Player* side, Geography* previous) {
  double flow = amount;
  double loss = Geography::traverseSupplies(amount, side, previous);
  if (previous) graphicsInfo->traverseSupplies(flow * (previous == vex1 ? 1 : -1), loss);
  return loss;
}

Hex* Hex::getTestHex (bool vi, bool fa, bool fo, bool mi) {
  static int called = 0;
  Hex* ret = new Hex(1000 * ++called, 1000, Plain);
  if (vi) ret->setVillage(Village::getTestVillage(1000));
  if (fa) ret->setFarm(Farmland::getTestFarm());
  if (fo) ret->setForest(Forest::getTestForest());
  if (mi) ret->setMine(Mine::getTestMine());
  ret->createVertices();
  for (VtxIterator vex = ret->vexBegin(); vex != ret->vexEnd(); ++vex) (*vex)->createLines();
  return ret;
}

int Hex::getTotalPopulation () const {
  if (village) return village->getTotalPopulation();
  return 0; 
}
