#include "StaticInitialiser.hh"
#include "Parser.hh"
#include "Player.hh" 
#include "Building.hh"
#include "MilUnit.hh" 
#include "Action.hh" 
#include "UtilityFunctions.hh" 
#include "GraphicsInfo.hh" 
#include <QGLFramebufferObject>
#include "CastleWindow.hh" 
#include <QGLShader>
#include <QGLShaderProgram>
#include "glextensions.h"
#include <GL/glu.h>
#include "Calendar.hh" 
#include <fstream>

int StaticInitialiser::defaultUnitPriority = 4; 

void StaticInitialiser::createCalculator (Object* info, Action::Calculator* ret) {
  assert(info);
  int dice = info->safeGetInt("dice", 1);
  int faces = info->safeGetInt("faces", 6);
  ret->die = new DieRoll(dice, faces);

  ret->results[VictoGlory] = info->safeGetInt("VictoGlory", 6);
  ret->results[Good]       = info->safeGetInt("Good", ret->results[VictoGlory] - 1);
  ret->results[Neutral]    = info->safeGetInt("Neutral", ret->results[Good] - 1);
  ret->results[Bad]        = info->safeGetInt("Bad", ret->results[Neutral] - 1);
  ret->results[Disaster]   = info->safeGetInt("Disaster", ret->results[Bad] - 1);

  assert(ret->results[VictoGlory] >= ret->results[Good]);
  assert(ret->results[Good] >= ret->results[Neutral]);
  assert(ret->results[Neutral] >= ret->results[Bad]);
  assert(ret->results[Bad] >= ret->results[Disaster]);
}

void StaticInitialiser::createActionProbabilities (Object* info) {
  assert(info);
  createCalculator(info->safeGetObject("attack"), Action::attackCalculator);
  createCalculator(info->safeGetObject("mobilise"), Action::mobiliseCalculator);
  createCalculator(info->safeGetObject("raid"), Action::devastateCalculator);
  createCalculator(info->safeGetObject("surrender"), Action::surrenderCalculator);
  createCalculator(info->safeGetObject("recruit"), Action::recruitCalculator);
  createCalculator(info->safeGetObject("colonise"), Action::colonyCalculator);  
}

void StaticInitialiser::loadAiConstants (Object* info) {
  if (!info) return;

  Player::influenceDecay      = info->safeGetFloat("influenceDecay",      Player::influenceDecay);
  Player::castleWeight        = info->safeGetFloat("castleWeight",        Player::castleWeight);
  Player::casualtyValue       = info->safeGetFloat("casualtyValue",       Player::casualtyValue); 
  Player::distanceModifier    = info->safeGetFloat("distanceModifier",    Player::distanceModifier);
  Player::distancePower       = info->safeGetFloat("distancePower",       Player::distancePower);
  Player::supplyWeight        = info->safeGetFloat("supplyWeight",        Player::supplyWeight);
  Player::siegeInfluenceValue = info->safeGetFloat("siegeInfluenceValue", Player::siegeInfluenceValue); 
}

void StaticInitialiser::overallInitialisation (Object* info) {
  Object* priorityLevels = info->safeGetObject("priorityLevels");
  vector<double> levels;
  if (priorityLevels) {
    for (int i = 0; i < priorityLevels->numTokens(); ++i) {
      levels.push_back(atof(priorityLevels->getToken(i).c_str()));
    }
  }
  MilUnit::setPriorityLevels(levels); 
  defaultUnitPriority = info->safeGetInt("defaultUnitPriority", 4);

  Calendar::setWeek(info->safeGetInt("week", 0));   
}

void StaticInitialiser::initialiseCivilBuildings (Object* popInfo) {
  assert(popInfo); 

  Castle::siegeModifier = popInfo->safeGetFloat("siegeModifier", Castle::siegeModifier);

  Farmland::_labourToSow    = popInfo->safeGetInt("labourToSow",    Farmland::_labourToSow);
  Farmland::_labourToPlow   = popInfo->safeGetInt("labourToPlow",   Farmland::_labourToPlow);
  Farmland::_labourToClear  = popInfo->safeGetInt("labourToClear",  Farmland::_labourToClear);
  Farmland::_labourToWeed   = popInfo->safeGetInt("labourToWeed",   Farmland::_labourToWeed);
  Farmland::_labourToReap   = popInfo->safeGetInt("labourToReap",   Farmland::_labourToReap);
  Farmland::_cropsFrom3     = popInfo->safeGetInt("cropsFrom3",     Farmland::_cropsFrom3);
  Farmland::_cropsFrom2     = popInfo->safeGetInt("cropsFrom2",     Farmland::_cropsFrom2);
  Farmland::_cropsFrom1     = popInfo->safeGetInt("cropsFrom1",     Farmland::_cropsFrom1);
  

  
  Object* femf = popInfo->safeGetObject("femaleFert");
  assert(femf);
  Object* pair = popInfo->safeGetObject("pairChance");
  assert(pair); 
  Object* femMort = popInfo->safeGetObject("femaleMort");
  assert(femMort);
  Object* malMort = popInfo->safeGetObject("maleMort");
  assert(malMort);  
  Object* prod = popInfo->safeGetObject("production");
  assert(prod); 
  Object* cons = popInfo->safeGetObject("consumption");
  assert(cons); 
  Object* recr = popInfo->safeGetObject("recruit");
  assert(recr); 
  
  double lastFmort = atof(femMort->getToken(0).c_str());
  double lastMmort = atof(malMort->getToken(0).c_str());
  double lastPair = atof(pair->getToken(0).c_str());
  double lastPreg = atof(femf->getToken(0).c_str());
  double lastProd = atof(prod->getToken(0).c_str());
  double lastCons = atof(cons->getToken(0).c_str());
  double lastRecr = atof(recr->getToken(0).c_str());  
  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    double curr = (femMort->numTokens() > i ? atof(femMort->getToken(i).c_str()) : lastFmort);
    CivilBuilding::baseFemaleMortality[i] = curr;
    lastFmort = curr;

    curr = (malMort->numTokens() > i ? atof(malMort->getToken(i).c_str()) : lastMmort);
    CivilBuilding::baseMaleMortality[i] = curr;
    lastMmort = curr;

    curr = (pair->numTokens() > i ? atof(pair->getToken(i).c_str()) : lastPair);
    CivilBuilding::pairChance[i] = curr;
    lastPair = curr;

    curr = (femf->numTokens() > i ? atof(femf->getToken(i).c_str()) : lastPreg);
    CivilBuilding::fertility[i] = curr;
    lastPreg = curr;

    curr = (prod->numTokens() > i ? atof(prod->getToken(i).c_str()) : lastProd);
    CivilBuilding::products[i] = curr;
    lastProd = curr;

    curr = (cons->numTokens() > i ? atof(cons->getToken(i).c_str()) : lastCons);
    CivilBuilding::consume[i] = curr;
    lastCons = curr;

    curr = (recr->numTokens() > i ? atof(recr->getToken(i).c_str()) : lastRecr);
    CivilBuilding::recruitChance[i] = curr;
    lastRecr = curr;
  }

  CivilBuilding::femaleProduction = popInfo->safeGetFloat("femaleProduction", CivilBuilding::femaleProduction);
  CivilBuilding::femaleConsumption = popInfo->safeGetFloat("femaleConsumption", CivilBuilding::femaleConsumption);
  CivilBuilding::femaleSurplusEffect = popInfo->safeGetFloat("femaleSurplusEffect", CivilBuilding::femaleSurplusEffect);
  CivilBuilding::femaleSurplusZero = popInfo->safeGetFloat("femaleSurplusZero", CivilBuilding::femaleSurplusZero);    
}

inline int heightMapWidth (int zoneSide) {
  return 2 + 3*zoneSide; 
}

inline int heightMapHeight (int zoneSide) {
  return 2 + heightMapWidth(zoneSide); // Additional 2 arises from hex/grid skew; check out (rightmost, downmost) RightDown vertex. 
}

void StaticInitialiser::initialiseGraphics (Object* gInfo) {
  //Logger::logStream(DebugStartup) << "StaticInitialiser::initialiseGraphics\n"; 

  new ZoneGraphicsInfo(); 
  GraphicsInfo::zoneSide = gInfo->safeGetInt("zoneSide", 4);
  int mapWidth = heightMapWidth(GraphicsInfo::zoneSide);
  int mapHeight = heightMapHeight(GraphicsInfo::zoneSide);
  GraphicsInfo::heightMap = new double[mapWidth*mapHeight];
  
  QImage b("gfx/heightmap.bmp");
  for (int x = 0; x < mapWidth; ++x) {    
    for (int y = 0; y < mapHeight; ++y) { 
      QRgb pix = b.pixel(x, y);
      GraphicsInfo::heightMap[mapWidth*y + x] = qRed(pix);
    }
  }
}

Hex* findHex (Object* info) {
  int x = info->safeGetInt("x", -1);
  if (0 > x) return 0;
  int y = info->safeGetInt("y", -1);
  if (0 > y) return 0;
  Hex* hex = Hex::getHex(x, y);
  return hex; 
}

Line* findLine (Object* info, Hex* hex) {
  string pos = info->safeGetString("pos", "nowhere");
  if (pos == "nowhere") return 0; 
  Direction dir = Hex::getDirection(pos);
  if (NoDirection == dir) return 0;
  assert(hex->getLine(dir)); 
  return hex->getLine(dir); 
}

Vertex* findVertex (Object* info, Hex* hex) {
  string pos = info->safeGetString("vtx", "nowhere");
  if (pos == "nowhere") return 0;   
  Vertices dir = Hex::getVertex(pos); 
  if (NoVertex == dir) return 0; 
  assert(hex->getVertex(dir)); 
  return hex->getVertex(dir); 
}

void StaticInitialiser::initialiseBuilding (Building* build, Object* info) {
  build->supplies = info->safeGetFloat("supplies", 0); 
}

void initialiseContract (ContractInfo* contract, Object* info) {
  if (!info) return; 
  if (!contract) return;

  contract->amount = info->safeGetFloat("amount", 0);
  if (info->safeGetString("type") == "fixed") contract->delivery = ContractInfo::Fixed;
  else if (info->safeGetString("type") == "percentage") contract->delivery = ContractInfo::Percentage; 
  else if (info->safeGetString("type") == "surplus_percentage") contract->delivery = ContractInfo::SurplusPercentage;
}

void StaticInitialiser::buildHex (Object* hInfo) {
  Hex* hex = findHex(hInfo);
  string ownername = hInfo->safeGetString("player");
  Player* owner = Player::findByName(ownername);
  if (owner) hex->setOwner(owner);
  
  Object* cinfo = hInfo->safeGetObject("castle");
  if (cinfo) {
    Line* lin = findLine(cinfo, hex);
    assert(0 == lin->getCastle());             
    Castle* castle = new Castle(hex, lin);
    castle->setOwner(owner);
    initialiseBuilding(castle, cinfo);
    initialiseContract(&(castle->taxExtraction), cinfo->safeGetObject("taxes"));
    castle->recruitType = MilUnitTemplate::getUnitType(cinfo->safeGetString("recruiting", *(MilUnitTemplate::beginTypeNames()))); 
    Object* garrison = cinfo->safeGetObject("garrison");
    if (garrison) {
      MilUnit* m = buildMilUnit(garrison); 
      m->setOwner(owner);
      castle->addGarrison(m);
    }
    lin->addCastle(castle); 
  }
  Object* fInfo = hInfo->safeGetObject("farmland");
  if (fInfo) {
    Farmland* farms = StaticInitialiser::buildFarm(fInfo);
    initialiseBuilding(farms, fInfo); 
    if (owner) farms->setOwner(owner);
    hex->setFarm(farms); 
  }  
}

void readAgeTrackerFromObject (AgeTracker& age, Object* obj) {
  if (!obj) return; 
  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    if (i >= obj->numTokens()) continue;
    age.addPop(obj->tokenAsInt(i), i);
    
  }
}

Farmland* StaticInitialiser::buildFarm (Object* fInfo) {
  Object* males = fInfo->safeGetObject("males");
  Object* females = fInfo->safeGetObject("females");

  Farmland* ret = new Farmland();
  readAgeTrackerFromObject(ret->males, males);
  readAgeTrackerFromObject(ret->women, females);

  buildMilitia(ret, fInfo->safeGetObject("militiaUnits")); 

  ret->fields[Farmland::Clear] = fInfo->safeGetInt("clear", 0);
  ret->fields[Farmland::Ready] = fInfo->safeGetInt("ready", 0);
  ret->fields[Farmland::Sowed] = fInfo->safeGetInt("sowed", 0);
  ret->fields[Farmland::Ripe1] = fInfo->safeGetInt("ripe1", 0);
  ret->fields[Farmland::Ripe2] = fInfo->safeGetInt("ripe2", 0);
  ret->fields[Farmland::Ripe3] = fInfo->safeGetInt("ripe3", 0);
  ret->fields[Farmland::Ended] = fInfo->safeGetInt("ended", 0);
  
  return ret;
}

void StaticInitialiser::buildMilitia (CivilBuilding* target, Object* mInfo) {
  if (0 == mInfo) return;
  for (MilUnitTemplate::Iterator i = MilUnitTemplate::begin(); i != MilUnitTemplate::end(); ++i) {
    assert(*i);     
    int amount = mInfo->safeGetInt((*i)->name);
    if (0 >= amount) continue;
    target->milTrad->militiaStrength[*i] = amount;
  }
  target->milTrad->drillLevel = mInfo->safeGetInt("drill_level"); 
}

MilUnit* StaticInitialiser::buildMilUnit (Object* mInfo) {
  if (!mInfo) return 0; 
  static AgeTracker ages; 
  
  MilUnit* m = new MilUnit();
  for (MilUnitTemplate::TypeNameIterator ut = MilUnitTemplate::beginTypeNames(); ut != MilUnitTemplate::endTypeNames(); ++ut) {
    Object* strength = mInfo->safeGetObject(*ut);
    if (!strength) continue;
    ages.clear();
    readAgeTrackerFromObject(ages, strength);
    for (int i = 0; i < 16; ++i) ages.age(); // Quick hack to avoid long strings of initial zeroes in every MilUnit definition. 
    
    m->addElement(MilUnitTemplate::getUnitType(*ut), ages);
    
  }
  m->setPriority(mInfo->safeGetInt("priority", defaultUnitPriority));

  Player* owner = Player::findByName(mInfo->safeGetString("player"));
  if (owner) m->setOwner(owner);
  Hex* hex = findHex(mInfo);
  if (hex) {
    Vertex* vtx = findVertex(mInfo, hex);
    if (vtx) vtx->addUnit(m);
  }
  m->setName(mInfo->safeGetString("name", "\"Unknown Soldiers\"")); 
  m->supplies = mInfo->safeGetFloat("supplies"); 
  
  return m; 
}

void StaticInitialiser::buildMilUnitTemplates (Object* info) {
  assert(info);
  MilUnit::defaultDecayConstant = info->safeGetFloat("defaultDecayConstant", MilUnit::defaultDecayConstant);
  Object* effects = info->safeGetObject("drillEffects");
  if (effects) {
    for (int i = 0; i < effects->numTokens(); ++i) {
      MilUnitTemplate::drillEffects.push_back(effects->tokenAsFloat(i));
    }
  }
  else {
    MilUnitTemplate::drillEffects.push_back(1.0);
    MilUnitTemplate::drillEffects.push_back(0.75);
    MilUnitTemplate::drillEffects.push_back(0.55);
    MilUnitTemplate::drillEffects.push_back(0.40);
    MilUnitTemplate::drillEffects.push_back(0.30);
    MilUnitTemplate::drillEffects.push_back(0.25);    
  }

  double mDrill = 1.0 / (MilUnitTemplate::drillEffects.size() - 1);
  int failNames = 0;

  objvec units = info->getValue("unit");
  for (objiter unit = units.begin(); unit != units.end(); ++unit) {
    string uname = (*unit)->safeGetString("name", "FAIL");
    if (uname == "FAIL") {
      sprintf(strbuffer, "FAIL UNIT TYPE %i", ++failNames);
      uname = strbuffer; 
    }
    
    MilUnitTemplate* nType   = new MilUnitTemplate(uname);
    nType->base_shock        = (*unit)->safeGetFloat("base_shock");
    nType->base_range        = (*unit)->safeGetFloat("base_range");
    nType->base_defense      = (*unit)->safeGetFloat("base_defense");
    nType->base_tacmob       = (*unit)->safeGetFloat("base_tacmob");
    nType->supplyConsumption = (*unit)->safeGetFloat("supplyConsumption");
    nType->recruit_speed     = (*unit)->safeGetInt("recruit_speed");
    nType->militiaDecay      = (*unit)->safeGetFloat("militiaDecay");
    nType->militiaDrill      = (*unit)->safeGetFloat("militiaDrill");

    if (nType->militiaDrill > mDrill) {
      /*
      Logger::logStream(Logger::Warning) << "Warning: Drill amount of unit "
					 << uname
					 << " set to more than "
					 << mDrill
					 << ", clamping.\n";
      */ 
      nType->militiaDrill = mDrill; 
	
    }
  }
}

void StaticInitialiser::createPlayer (Object* info) {
  static int numFactions = 0;
  
  bool human = (info->safeGetString("human", "no") == "yes");
  sprintf(strbuffer, "\"faction_auto_name_%i\"", numFactions++); 
  string name = info->safeGetString("name", strbuffer);
  string display = remQuotes(info->safeGetString("displayname", name));
  Player* ret = new Player(human, display, name);
  ret->graphicsInfo = new PlayerGraphicsInfo();

  int red = info->safeGetInt("red");
  int green = info->safeGetInt("green");
  int blue = info->safeGetInt("blue");
  ret->graphicsInfo->colour = qRgb(red, green, blue); 
}

double StaticInitialiser::interpolate (double xfrac, double yfrac, int mapWidth, int mapHeight, double* heightMap) {
  const double binWidth = 1.0 / mapWidth;
  const double binHeight = 1.0 / mapHeight; 
  
  int xbin = (int) floor(xfrac * mapWidth);  
  int ybin = (int) floor(yfrac * mapHeight);
  int nextXbin = min(xbin+1, mapWidth-1);
  int nextYbin = min(ybin+1, mapHeight-1); 
  
  double height1 = heightMap[ybin*mapWidth + xbin];
  double height2 = heightMap[ybin*mapWidth + nextXbin];
  double height3 = heightMap[nextYbin*mapWidth + xbin];
  double height4 = heightMap[nextYbin*mapWidth + nextXbin];  

  xfrac -= binWidth*xbin;     
  xfrac /= binWidth;
  yfrac -= binHeight*ybin;
  yfrac /= binHeight; 

  double ret = (1-xfrac)*(1-yfrac) * height1;
  ret       +=    xfrac *(1-yfrac) * height2;
  ret       += (1-xfrac)*   yfrac  * height3;
  ret       +=    xfrac *   yfrac  * height4;

  return ret; 
}

void StaticInitialiser::addShadows (QGLFramebufferObject* fbo, int texture) {
  static bool shaded[GraphicsInfo::zoneSize];
  static double lightAngle = tan(30.0 / 180.0 * M_PI); 
  static double invSize = 2.0 / GraphicsInfo::zoneSize;

  ZoneGraphicsInfo* zoneInfo = ZoneGraphicsInfo::getZoneInfo(0);

  // Seed detailed heightmap using coarse one. 
  for (int yval = 0; yval < GraphicsInfo::zoneSize; ++yval) {
    for (int xval = 0; xval < GraphicsInfo::zoneSize; ++xval) {
      zoneInfo->heightMap[xval][yval] = interpolate(((double) xval)/GraphicsInfo::zoneSize,
						    ((double) yval)/GraphicsInfo::zoneSize,
						    heightMapWidth(GraphicsInfo::zoneSide),
						    heightMapHeight(GraphicsInfo::zoneSide),			    
						    GraphicsInfo::heightMap);
    }
  }
  
  // Square/diamond fractal
  vector<QRect> squares;
  static const int modSize = 129;
  QRect bigSquare(0, 0, modSize-1, modSize-1); 
  squares.push_back(bigSquare);

  // From QRect doc for bottom():
  // Note that for historical reasons this function returns top() + height() - 1; use y() + height() to retrieve the true y-coordinate.
  // Similarly for right(). Hence ugly +1s that appear below. 

  static double modulate[modSize*modSize];
  static int touched[modSize][modSize];
  for (int i = 0; i < modSize; ++i) {
    for (int j = 0; j < modSize; ++j) {
      modulate[j*modSize + i] = 0;
      touched[i][j] = 0;
    }
  }
  
  static const double midValue = 1500;
  static const double dimValue = 750; 
  static const double eConst = 0; 
  static const double eBias  = 0.5;
  static const int cutoff = 2;
  static const double iterPower = 1; 

  int iter = 1; 
  while (0 < squares.size()) {
    QRect curr = squares.back();
    squares.pop_back();
    if (cutoff > curr.width()) continue;
    if (cutoff > curr.height()) continue;     
    QPoint midpoint(curr.left() + curr.width()/2, curr.top() + curr.height()/2); 

    // Set 'diamond' points
    QPoint north(midpoint.x(), curr.top());
    double newValue = 0; 
    newValue += modulate[curr.top()*modSize + curr.left()];
    newValue += modulate[(curr.top())*modSize + curr.right()+1];
    newValue += (curr.top() - midpoint.y() >= 0 ? modulate[(curr.top() - midpoint.y())*modSize + midpoint.x()] : 0);
    newValue += modulate[(midpoint.y())*modSize + midpoint.x()];
    newValue *= 0.25;
    
    double extra = rand();
    extra /= RAND_MAX;
    extra -= eBias;    
    extra *= eConst + (dimValue * curr.width());
    extra /= GraphicsInfo::zoneSize;
    extra /= pow(iter, iterPower);    
    if (0 <= touched[north.x()][north.y()]) {
      modulate[(north.y())*modSize + north.x()] = newValue + extra;
      touched[north.x()][north.y()]++;
    }

    QPoint south(midpoint.x(), curr.bottom()+1);
    newValue = 0; 
    newValue += modulate[(curr.bottom()+1)*modSize + curr.left()];
    newValue += modulate[(curr.bottom()+1)*modSize + curr.right()+1];
    newValue += modulate[(midpoint.y())*modSize + midpoint.x()];        
    newValue += (midpoint.y() + curr.height() < modSize ? modulate[(midpoint.y() + curr.height())*modSize + midpoint.x()] : 0);
    newValue *= 0.25;
      
    extra = rand();
    extra /= RAND_MAX;
    extra -= eBias;    
    extra *= eConst + (dimValue * curr.width());
    extra /= GraphicsInfo::zoneSize;
    extra /= pow(iter, iterPower);
    if (0 <= touched[south.x()][south.y()]) {    
      modulate[(south.y())*modSize + south.x()] = newValue + extra;
      touched[south.x()][south.y()]++;
    }
    
    QPoint west(curr.left(), midpoint.y());
    newValue = 0; 
    newValue += modulate[(curr.top())*modSize + curr.left()];
    newValue += modulate[(curr.bottom()+1)*modSize + curr.left()];
    newValue += modulate[(midpoint.y())*modSize + midpoint.x()];        
    newValue += (midpoint.x() - curr.width() >= 0 ? modulate[(midpoint.y())*modSize + midpoint.x() - curr.width()] : 0);    
    newValue *= 0.25;
      
    extra = rand();
    extra /= RAND_MAX;
    extra -= eBias;    
    extra *= eConst + (dimValue * curr.width());
    extra /= GraphicsInfo::zoneSize;
    extra /= pow(iter, iterPower);
    if (0 <= touched[west.x()][west.y()]) {        
      modulate[(west.y())*modSize + west.x()] = newValue + extra;
      touched[west.x()][west.y()]++;
    }

    QPoint east(curr.right()+1, midpoint.y());
    newValue = 0; 
    newValue += modulate[(curr.top())*modSize + curr.right()+1];
    newValue += modulate[(curr.bottom()+1)*modSize + curr.right()+1];
    newValue += modulate[(midpoint.y())*modSize + midpoint.x()];            
    newValue += (midpoint.x() + curr.width() < modSize ? modulate[(midpoint.y())*modSize + midpoint.x() + curr.width()] : 0);        
    newValue *= 0.25;
      
    extra = rand();
    extra /= RAND_MAX;
    extra -= eBias;    
    extra *= eConst + (dimValue * curr.width());
    extra /= GraphicsInfo::zoneSize;
    extra /= pow(iter, iterPower);
    if (0 <= touched[east.x()][east.y()]) {            
      modulate[(east.y())*modSize + east.x()] = newValue + extra;
      touched[east.x()][east.y()]++;
    }

    // Midpoint 
    newValue = 0; 
    newValue += modulate[(curr.top())*modSize + curr.left()];
    newValue += modulate[(curr.bottom()+1)*modSize + curr.left()];
    newValue += modulate[(curr.top())*modSize + curr.right()+1];
    newValue += modulate[(curr.bottom()+1)*modSize + curr.right()+1];
    newValue *= 0.25;
    
    extra = rand();
    extra /= RAND_MAX;
    extra -= eBias;
    extra *= eConst + (midValue * curr.width());
    extra /= GraphicsInfo::zoneSize;
    extra /= pow(iter, iterPower);
    if (0 <= touched[midpoint.x()][midpoint.y()]) {                
      modulate[(midpoint.y())*modSize + midpoint.x()] = newValue + extra;
      touched[midpoint.x()][midpoint.y()]++;
    }

    // Generate new squares
    squares.push_back(QRect(curr.left(), curr.top(), (midpoint.x() - curr.left()), (midpoint.y() - curr.top())));
    squares.push_back(QRect(curr.left(), midpoint.y(), (midpoint.x() - curr.left()), (curr.bottom()+1 - midpoint.y())));
    squares.push_back(QRect(midpoint.x(), curr.top(), (curr.right()+1 - midpoint.x()), (midpoint.y() - curr.top())));
    squares.push_back(QRect(midpoint.x(), midpoint.y(), (curr.right()+1 - midpoint.x()), (curr.bottom()+1 - midpoint.y())));
    iter++; 
  }

  
  for (int yval = 0; yval < GraphicsInfo::zoneSize; ++yval) {
    for (int xval = 0; xval < GraphicsInfo::zoneSize; ++xval) {  
      double xfrac = xval;
      xfrac /= GraphicsInfo::zoneSize;
      double yfrac = yval;
      yfrac /= GraphicsInfo::zoneSize;
      zoneInfo->heightMap[xval][yval] += interpolate(xfrac, yfrac, modSize, modSize, modulate);
    }
  }

  
  for (int yval = 0; yval < GraphicsInfo::zoneSize; ++yval) {
    // First calculate all the point heights. 
    for (int xval = 0; xval < GraphicsInfo::zoneSize; ++xval) {
      shaded[xval] = false; 
    }

    // For each point, throw a shadow to the right.     
    for (int xval = 0; xval < GraphicsInfo::zoneSize; ++xval) {
      double xHeight = zoneInfo->heightMap[xval][yval];
      for (int cand = xval + 1; cand < GraphicsInfo::zoneSize; ++cand) {
	if (shaded[cand]) continue;
	double candHeight = zoneInfo->heightMap[cand][yval];
	if (cand - xval > (xHeight - candHeight)*lightAngle) continue; // X point does not shade this candidate.
	shaded[cand] = true;
      }
    }

    // Draw the shadow texture and scale to graphics step. 
    for (int xval = 0; xval < GraphicsInfo::zoneSize; ++xval) {
      zoneInfo->heightMap[xval][yval] *= GraphicsInfo::zSeparation; 
      if (!shaded[xval]) continue;
      QRectF point(xval*invSize - 1, yval*invSize - 1, invSize * 1.5, invSize * 1.5);
      fbo->drawTexture(point, texture); 
    }
  }
  
}

void createTexture (QGLFramebufferObject* fbo, int minHeight, int maxHeight, double* heightMap, int mapWidth, int texture) {
  // Corners of texture are at (-1, -1) and (1, 1) because drawTexture uses model space
  // and the glOrtho call above. 

  int repeats = 3;
  
  double xstep = 2;
  xstep /= (mapWidth-1); // Not calculating bin centers. Last bin edge should be on GraphicsInfo::zoneSize, or 2 in model space.
  double ystep = 2;
  ystep /= (mapWidth+2-1);

  xstep /= repeats;
  ystep /= repeats;
  
  const static double overlap = 2.00;   

  for (int x = 0; x < mapWidth; ++x) {
    for (int y = 0; y < mapWidth+2; ++y) {
      if (heightMap[y*mapWidth + x] < minHeight) continue;
      if (heightMap[y*mapWidth + x] > maxHeight) continue;


      for (int i = 0; i < repeats; ++i) {
	double xCenter = -1 + (3*x + i)*xstep;  
	for (int j = 0; j < repeats; ++ j) {
	  glLoadIdentity();
	  
	  double yCenter = -1 + (3*y + j)*ystep; // Notice positive y is up, opposite of heightmap
	  double angle = rand();
	  angle /= RAND_MAX;
	  angle *= 360;
	  // Rotation comes second because matrix multiplication reverses the order.
	  glTranslated(xCenter + 0.5*overlap*xstep, yCenter + 0.5*overlap*ystep, 0);
	  glRotated(angle, 0, 0, 1);      
	  
	  // Above calculation of step sizes is exact. Multiply texture size by constant factor to create overlap.      
	  fbo->drawTexture(QRectF(-0.5*overlap*xstep, -0.5*overlap*ystep, overlap*xstep, overlap*ystep), texture);
	}
      }
    }
  }
}

void StaticInitialiser::makeZoneTextures (Object* ginfo) {
  GLDrawer* hexDrawer = WarfareWindow::currWindow->hexDrawer;  

  //const char* names[NoTerrain] = {"mountain.bmp", "hill.bmp", "gfx\\grass.png", "forest.bmp", "ocean.bmp"};
  //QColor colours[NoTerrain] = {Qt::gray, Qt::lightGray, Qt::yellow, Qt::green, Qt::blue};
  
  QGLFramebufferObjectFormat format;
  format.setAttachment(QGLFramebufferObject::NoAttachment);
  // Neither depth nor stencil needed. Stencil causes major issue where
  // areas that have been drawn to can't be drawn to again, which completely
  // scuppers the intended overlap. 
  format.setInternalTextureFormat(GL_RGBA); 
  QGLFramebufferObject* fbo = new QGLFramebufferObject(GraphicsInfo::zoneSize, GraphicsInfo::zoneSize, format);
  fbo->bind();
  glViewport(0, 0, fbo->size().width(), fbo->size().height());
  glEnable(GL_TEXTURE_2D);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); // Just use the texture!
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);  
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA_SATURATE, GL_DST_ALPHA); 

  /*
  QGLShader alphaWeightedBlender(QGLShader::Fragment);
  QByteArray sourceCode;

  sourceCode.append("#version 120\n");
  sourceCode.append("uniform sampler2D tex0;\n");
  sourceCode.append("uniform sampler2D tex1;\n");  
  sourceCode.append("void main(void){\n");
  sourceCode.append("  vec4 t0 = texture2D(tex0, gl_TexCoord[0].st);\n");
  sourceCode.append("  vec4 t1 = texture2D(tex1, vec2(gl_TexCoord[0].s, 1.0 - gl_TexCoord[0].t));\n"); // Blit is upside down relative to drawTexture? 
  sourceCode.append("  float t0_alpha = t0.a;\n");
  sourceCode.append("  float t1_alpha = t1.a;\n");
  sourceCode.append("  float t0_weight = t0_alpha / (t0_alpha + t1_alpha);\n");
  sourceCode.append("  gl_FragColor = mix(t1, t0, t0_weight);\n"); 
  sourceCode.append("  gl_FragColor = vec4(t0_weight, (1.0 - t0_weight), 0.0, 1.0);\n");
  //sourceCode.append("  gl_FragColor = t0;\n"); 
  sourceCode.append("}\n");

  //sourceCode.append("");
  //sourceCode.append("");
  bool success = alphaWeightedBlender.compileSourceCode(sourceCode);
  if (!success) Logger::logStream(DebugStartup) << "Failed to compile source code\n"
						 << alphaWeightedBlender.sourceCode().data()
						 << "\n";

  QGLShaderProgram shadProg;
  success = shadProg.addShader(&alphaWeightedBlender); if (!success) Logger::logStream(DebugStartup) << "Failed to add shader.\n";  
  success = shadProg.link();                           if (!success) Logger::logStream(DebugStartup) << "Failed to link shader.\n";
  success = shadProg.bind();                           if (!success) Logger::logStream(DebugStartup) << "Failed to bin shader.\n";
  */
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1.0, 1.0, -1.0, 1.0, 99, -99);
  gluLookAt(0.0, 0.0, 1.0,  // Stand at 0, 0, 1. (glOrtho will not distort the edges.) 
	    0.0, 0.0, 0.0,  // Look at origin. 
	    0.0, 1.0, 0.0); // Up is positive y direction.
  
  
  glBindTexture(GL_TEXTURE_2D, fbo->texture()); 
  glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
 
  glMatrixMode(GL_MODELVIEW);

  Object* terrainTextures = ginfo->safeGetObject("terrainTextures");
  assert(terrainTextures);
  objvec terrains = terrainTextures->getValue("height");
  assert(terrains.size());
  hexDrawer->terrainTextureIndices = new int[terrains.size()];
  hexDrawer->zoneTextures = new int[1];
  for (unsigned int i = 0; i < terrains.size(); ++i) {
    hexDrawer->terrainTextureIndices[i] = hexDrawer->loadTexture(remQuotes(terrains[i]->safeGetString("file")), Qt::blue);
    int minHeight = terrains[i]->safeGetInt("minimum");
    int maxHeight = terrains[i]->safeGetInt("maximum");    
    createTexture(fbo, minHeight, maxHeight, GraphicsInfo::heightMap, heightMapWidth(GraphicsInfo::zoneSide), hexDrawer->textureIDs[hexDrawer->terrainTextureIndices[i]]);     
  }


  int shadowTexture = hexDrawer->assignTextureIndex();
  // 2x2 matrix of half-transparent black. 
  uchar bits[16] = {0};
  bits[3] = bits[7] = bits[11] = bits[15] = 128;
  
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);   
  glLoadIdentity();  
  glBindTexture(GL_TEXTURE_2D, shadowTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, bits);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
  addShadows(fbo, shadowTexture);

  fbo->release();
  hexDrawer->setViewport();
  hexDrawer->zoneTextures[0] = fbo->texture();  

  //shadProg.release(); 
  
  //Logger::logStream(DebugStartup) << "Exiting makeZoneTextures" << QThread::currentThreadId() << "\n"; 
}

void StaticInitialiser::writeUnitToObject (MilUnit* unit, Object* obj) {
  obj->setLeaf("name", unit->getName());
  obj->setLeaf("player", unit->getOwner()->getName());   
  for (vector<MilUnitElement*>::iterator i = unit->forces.begin(); i != unit->forces.end(); ++i) {
    if (1 > (*i)->strength()) continue;
    Object* numbers = new Object((*i)->unitType->name);
    obj->setValue(numbers);
    writeAgeInfoToObject(*((*i)->soldiers), numbers, 16); 
  }
  if (0 < unit->supplies) obj->setLeaf("supplies", unit->supplies);
  obj->setLeaf("priority", unit->priority);
}

void StaticInitialiser::writeAgeInfoToObject (AgeTracker& age, Object* obj, int skip) {
  int firstZero = age.people.size();
  for (int i = firstZero-1; i >= 0; --i) {
    if (age.people[i] > 0) break;
    firstZero = i; 
  }
  
  for (int i = skip; i < firstZero; ++i) {
    obj->addToList(age.people[i]); 
  }
}

void StaticInitialiser::writeGameToFile (string fname) {
  Object* game = new Object("game");
  Parser::topLevel = game;
  game->setLeaf("week", Calendar::currentWeek());
  Object* pLevels = new Object("priorityLevels");
  pLevels->setObjList();
  for (vector<double>::iterator i = MilUnit::priorityLevels.begin(); i != MilUnit::priorityLevels.end(); ++i) {
    pLevels->addToList(*i); 
  }
  game->setValue(pLevels);
  game->setLeaf("defaultPriority", defaultUnitPriority); 

  for (Player::Iterator p = Player::begin(); p != Player::end(); ++p) {
    Object* faction = new Object("faction");
    faction->setLeaf("name", (*p)->getName());
    faction->setLeaf("displayname", string("\"") + (*p)->getDisplayName() + "\"");
    faction->setLeaf("human", (*p)->isHuman() ? "yes" : "no");
    PlayerGraphicsInfo const* pgInfo = (*p)->getGraphicsInfo();
    faction->setLeaf("red",   pgInfo->getRed());
    faction->setLeaf("green", pgInfo->getGreen());
    faction->setLeaf("blue",  pgInfo->getBlue());    
    
    game->setValue(faction); 
  }

  game->setLeaf("currentplayer", WarfareWindow::currWindow->currentPlayer->getName()); 

  int maxx = -1;
  int maxy = -1;
  for (Hex::Iterator hex = Hex::begin(); hex != Hex::end(); ++hex) {
    maxx = max((*hex)->getPos().first, maxx);
    maxy = max((*hex)->getPos().second, maxy);
  }

  Object* hexgrid = new Object("hexgrid");
  hexgrid->setLeaf("x", maxx+1);
  hexgrid->setLeaf("y", maxy+1);
  game->setValue(hexgrid); 

  for (Hex::Iterator hex = Hex::begin(); hex != Hex::end(); ++hex) {
    Object* hexInfo = new Object("hexinfo");
    game->setValue(hexInfo);
    hexInfo->setLeaf("x", (*hex)->getPos().first);
    hexInfo->setLeaf("y", (*hex)->getPos().second);

    for (Hex::LineIterator lin = (*hex)->linBegin(); lin != (*hex)->linEnd(); ++lin) {
      Castle* castle = (*lin)->getCastle();
      if (!castle) continue;
      if (castle->getSupport() != (*hex)) continue;
      hexInfo->setLeaf("player", castle->getOwner()->getName());
      Object* castleObject = new Object("castle");
      hexInfo->setValue(castleObject);
      castleObject->setLeaf("pos", Hex::getDirectionName((*hex)->getDirection(*lin)));
      castleObject->setLeaf("supplies", castle->supplies);
      castleObject->setLeaf("recruiting", castle->recruitType->name); 
      for (unsigned int i = 0; (int) i < castle->numGarrison(); ++i) {
	Object* garrObject = new Object("garrison");
	castleObject->setValue(garrObject);
	writeUnitToObject(castle->getGarrison(i), garrObject);
      }
      Object* taxObject = new Object("taxes");
      castleObject->setValue(taxObject);
      taxObject->setLeaf("amount", castle->taxExtraction.amount);
      switch (castle->taxExtraction.delivery) {
      default: 
      case ContractInfo::Fixed:             taxObject->setLeaf("type", "fixed"); break;
      case ContractInfo::Percentage:        taxObject->setLeaf("type", "percentage"); break;
      case ContractInfo::SurplusPercentage: taxObject->setLeaf("type", "surplus_percentage"); break;
      }
    }

    Farmland* farm = (*hex)->getFarm();
    if (farm) {
      Object* farmInfo = new Object("farmland");
      hexInfo->setValue(farmInfo);
      Object* males = new Object("males");
      writeAgeInfoToObject(farm->males, males);
      farmInfo->setValue(males);

      males = new Object("females");      
      writeAgeInfoToObject(farm->women, males);
      farmInfo->setValue(males);
      
      farmInfo->setLeaf("supplies", farm->supplies);
      farmInfo->setLeaf("clear", farm->getFieldStatus(Farmland::Clear));
      farmInfo->setLeaf("ready", farm->getFieldStatus(Farmland::Ready));
      farmInfo->setLeaf("sowed", farm->getFieldStatus(Farmland::Sowed));
      farmInfo->setLeaf("ripe1", farm->getFieldStatus(Farmland::Ripe1));
      farmInfo->setLeaf("ripe2", farm->getFieldStatus(Farmland::Ripe2));
      farmInfo->setLeaf("ripe3", farm->getFieldStatus(Farmland::Ripe3));
      farmInfo->setLeaf("ended", farm->getFieldStatus(Farmland::Ended));      
      
    }
  }

  for (Vertex::Iterator vtx = Vertex::begin(); vtx != Vertex::end(); ++vtx) {
    if (0 == (*vtx)->numUnits()) continue;
    MilUnit* unit = (*vtx)->getUnit(0); 
    Object* uinfo = new Object("unit");
    int counter = 0;
    Hex* hex = (*vtx)->getHex(counter++);
    while (!hex) {
      hex = (*vtx)->getHex(counter++);
      assert(counter <= 6); 
    }
    uinfo->setLeaf("x", hex->getPos().first);
    uinfo->setLeaf("y", hex->getPos().second);
    uinfo->setLeaf("player", unit->getOwner()->getName());
    uinfo->setLeaf("vtx", Hex::getVertexName(hex->getDirection(*vtx)));
    writeUnitToObject(unit, uinfo); 
    game->setValue(uinfo);
  }
 
  ofstream writer;
  writer.open(fname.c_str());
  writer << (*game) << std::endl;
  writer.close(); 
}
