#include "StaticInitialiser.hh"
#include "Parser.hh"
#include "Player.hh" 
#include "Building.hh"
#include "Market.hh"
#include "MilUnit.hh" 
#include "Action.hh"
#include "EconActor.hh" 
#include "UtilityFunctions.hh" 
#include "GraphicsInfo.hh" 
#include <QGLFramebufferObject>
#include "CastleWindow.hh" 
#include <QGLShader>
#include <QGLShaderProgram>
#include "glextensions.h"
#include <GL/glu.h>
#include "Calendar.hh"
#include "Directions.hh" 
#include <fstream>

int StaticInitialiser::defaultUnitPriority = 4; 
ThreeDSprite* makeSprite (Object* info); 
map<MilUnitTemplate*, Object*> milUnitSpriteObjectMap;

void readGoodsHolder (Object* goodsObject, GoodsHolder& goods) {
  goods.clear();
  if (!goodsObject) return;
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {    
    goods.deliverGoods((*tg), goodsObject->safeGetFloat((*tg)->getName()));
  }
}

void StaticInitialiser::createCalculator (Object* info, Action::Calculator* ret) {
  if (!info) throw string("Expected info object to create calculator, got null.");
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
  Logger::logStream(DebugStartup) << __FILE__ << " " << __LINE__ << "\n";  
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
  Logger::logStream(DebugStartup) << __FILE__ << " " << __LINE__ << "\n";  
}

GLuint loadTexture (string fname, QColor backup, GLuint index) {
  QImage b;
  if (!b.load(fname.c_str())) {
    b = QImage(32, 32, QImage::Format_RGB888);
    b.fill(backup.rgb());
    Logger::logStream(DebugStartup) << "Failed to load " << fname << "\n"; 
  }
  
  QImage t = QGLWidget::convertToGLFormat(b);
  if (0 == index) glGenTextures(1, &index); // 0 is default, indicating "make me a new one". 
  
  glBindTexture(GL_TEXTURE_2D, index); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.width(), t.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.bits());

  return index;
}

void StaticInitialiser::graphicsInitialisation () {
  loadSprites();  
  for (MilUnit::Iterator m = MilUnit::start(); m != MilUnit::final(); ++m) {
    (*m)->graphicsInfo->updateSprites(*m);
  }
  
  for (Hex::Iterator h = Hex::start(); h != Hex::final(); ++h) {
    Village* f = (*h)->getVillage();
    if (!f) continue;
    if (!f->milTrad) continue;
    if (!f->milTrad->militia) continue;
    f->milTrad->militia->graphicsInfo->updateSprites(f->milTrad);
  }

  for (Player::Iter p = Player::start(); p != Player::final(); ++p) {
    string pName = "gfx/" + (*p)->getName() + ".png";
    GLuint texid; 
    glGenTextures(1, &texid);
    texid = loadTexture(pName, Qt::red, texid); 
    WarfareWindow::currWindow->hexDrawer->playerToTextureMap[*p] = texid; 
  }
}

template<class T> void StaticInitialiser::initialiseIndustry(Object* industryObject) {
  T::output = TradeGood::getByName(industryObject->safeGetString("output", "nosuchbeast"));
  if (!T::output) throw string("No output specified for industry");
  T::capital = new GoodsHolder();
  Object* capInfo = industryObject->getNeededObject("capital");
  readGoodsHolder(capInfo, *(T::capital));
  T::inverseProductionTime = 1.0 / industryObject->safeGetInt("productionCycle", 1);
}

void StaticInitialiser::initialiseCivilBuildings (Object* popInfo) {
  assert(popInfo);
  
  initialiseMaslowHierarchy(popInfo->safeGetObject("pop_needs")); 
  Castle::siegeModifier = popInfo->safeGetFloat("siegeModifier", Castle::siegeModifier);

  Object* farmInfo          = popInfo->getNeededObject("farmland");
  FieldStatus::initialise();
  Farmer::_labourToSow    = farmInfo->safeGetInt("labourToSow",    Farmer::_labourToSow);
  Farmer::_labourToPlow   = farmInfo->safeGetInt("labourToPlow",   Farmer::_labourToPlow);
  Farmer::_labourToClear  = farmInfo->safeGetInt("labourToClear",  Farmer::_labourToClear);
  Farmer::_labourToWeed   = farmInfo->safeGetInt("labourToWeed",   Farmer::_labourToWeed);
  Farmer::_labourToReap   = farmInfo->safeGetInt("labourToReap",   Farmer::_labourToReap);
  Farmer::_cropsFrom3     = farmInfo->safeGetInt("cropsFrom3",     Farmer::_cropsFrom3);
  Farmer::_cropsFrom2     = farmInfo->safeGetInt("cropsFrom2",     Farmer::_cropsFrom2);
  Farmer::_cropsFrom1     = farmInfo->safeGetInt("cropsFrom1",     Farmer::_cropsFrom1);
  initialiseIndustry<Farmer>(farmInfo);
  
  Object* forestInfo       = popInfo->getNeededObject("forest");
  ForestStatus::initialise();
  Forest::_labourToTend    = forestInfo->safeGetInt("labourToTend", Forest::_labourToTend);
  Forest::_labourToHarvest = forestInfo->safeGetInt("labourToHarvest", Forest::_labourToHarvest);
  Forest::_labourToClear   = forestInfo->safeGetInt("labourToClear", Forest::_labourToClear);
  Object* amountOfWood     = forestInfo->getNeededObject("amountOfWood");
  Forest::_amountOfWood.clear();
  Forest::_amountOfWood.push_back(0);
  Forest::_amountOfWood.push_back(0);
  Forest::_amountOfWood.push_back(1);
  Forest::_amountOfWood.push_back(2);
  Forest::_amountOfWood.push_back(5);
  Forest::_amountOfWood.push_back(10);
  Forest::_amountOfWood.push_back(25);
  Forest::_amountOfWood.push_back(50);
  Forest::_amountOfWood.push_back(100);
  Forest::_amountOfWood.push_back(250);
  for (int i = 0; i < amountOfWood->numTokens(); ++i) {
    Forest::_amountOfWood[i] = amountOfWood->tokenAsInt(i);
  }
  initialiseIndustry<Forester>(forestInfo);
  
  Object* mineInfo       = popInfo->getNeededObject("mine");
  Mine::_amountOfIron    = mineInfo->safeGetInt("amount", Mine::_amountOfIron);
  initialiseIndustry<Mine::Miner>(mineInfo);
  Enumerable<Mine::MineStatus>::clear();
  objvec statuses = mineInfo->getValue("status");
  for (unsigned int i = 0; i < statuses.size(); ++i) {
    Mine::MineStatus* stat = new Mine::MineStatus(statuses[i]->safeGetString("name", "nameNotFound"), statuses[i]->safeGetInt("requiredLabour", 10), (i+1) == statuses.size());
    stat = 0; //Quiet, compiler
  }
  
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
    Village::baseFemaleMortality[i] = curr;
    lastFmort = curr;

    curr = (malMort->numTokens() > i ? atof(malMort->getToken(i).c_str()) : lastMmort);
    Village::baseMaleMortality[i] = curr;
    lastMmort = curr;

    curr = (pair->numTokens() > i ? atof(pair->getToken(i).c_str()) : lastPair);
    Village::pairChance[i] = curr;
    lastPair = curr;

    curr = (femf->numTokens() > i ? atof(femf->getToken(i).c_str()) : lastPreg);
    Village::fertility[i] = curr;
    lastPreg = curr;

    curr = (prod->numTokens() > i ? atof(prod->getToken(i).c_str()) : lastProd);
    Village::products[i] = curr;
    lastProd = curr;

    curr = (cons->numTokens() > i ? atof(cons->getToken(i).c_str()) : lastCons);
    Village::consume[i] = curr * 0.25;
    lastCons = curr;

    curr = (recr->numTokens() > i ? atof(recr->getToken(i).c_str()) : lastRecr);
    Village::recruitChance[i] = curr;
    lastRecr = curr;
  }

  Village::femaleProduction = popInfo->safeGetFloat("femaleProduction", Village::femaleProduction);
  Village::femaleConsumption = popInfo->safeGetFloat("femaleConsumption", Village::femaleConsumption);
  Village::femaleSurplusEffect = popInfo->safeGetFloat("femaleSurplusEffect", Village::femaleSurplusEffect);
  Village::femaleSurplusZero = popInfo->safeGetFloat("femaleSurplusZero", Village::femaleSurplusZero);    
}

void initialiseContract (ContractInfo* contract, Object* info) {
  if (!info) return; 
  if (!contract) return;

  contract->recipient = EconActor::getByIndex(info->safeGetUint("target")); 
  contract->amount = info->safeGetFloat("amount", 0);
  if (info->safeGetString("type") == "fixed") contract->delivery = ContractInfo::Fixed;
  else if (info->safeGetString("type") == "percentage") contract->delivery = ContractInfo::Percentage; 
  else if (info->safeGetString("type") == "surplus_percentage") contract->delivery = ContractInfo::SurplusPercentage;
  contract->tradeGood = TradeGood::getByName(info->safeGetString("good"));
}

void StaticInitialiser::initialiseEcon (EconActor* econ, Object* info) {
  unsigned int id = info->safeGetUint("id");
  if (EconActor::getByIndex(id)) {
    Logger::logStream(DebugStartup) << "Bad econ id " << id << " " << info << " already exists.\n";
    assert(!EconActor::getByIndex(id)); 
  }
  econ->setIdx(id);

  static map<Object*, ContractInfo*> unFilled;
  objvec toRemove;
  for (map<Object*, ContractInfo*>::iterator un = unFilled.begin(); un != unFilled.end(); ++un) {
    if ((*un).first->safeGetUint("recipient") != id) continue;
    (*un).second->recipient = econ;
    toRemove.push_back((*un).first);
  }
  for (objiter tr = toRemove.begin(); tr != toRemove.end(); ++tr) unFilled.erase(*tr); 
  
  objvec contracts = info->getValue("contract");
  for (objiter cInfo = contracts.begin(); cInfo != contracts.end(); ++cInfo) {
    ContractInfo* contract = new ContractInfo();
    initialiseContract(contract, *cInfo);
    if (!contract->recipient) unFilled[*cInfo] = contract;
    contract->source = econ;
    econ->obligations.push_back(contract);
  }

  readGoodsHolder(info->safeGetObject("goods"), *econ);
  int ownerIdx = info->safeGetInt("owner", -1);
  if (ownerIdx >= 0) econ->setEconOwner(EconActor::getByIndex(ownerIdx));
}

inline int heightMapWidth (int zoneSide) {
  return 2 + 3*zoneSide; 
}

inline int heightMapHeight (int zoneSide) {
  return 2 + heightMapWidth(zoneSide); // Additional 2 arises from hex/grid skew; check out (rightmost, downmost) RightDown vertex. 
}

void StaticInitialiser::initialiseGoods (Object* gInfo) {
  TradeGood::initialise();

  objvec goods = gInfo->getLeaves();
  for (objiter go = goods.begin(); go != goods.end(); ++go) {
    string goodName = (*go)->getKey();
    TradeGood* tradeGood = (TradeGood*) TradeGood::Labor;
    if (goodName != tradeGood->getName()) tradeGood = new TradeGood(goodName, (*go) == goods.back());
    tradeGood->stickiness  = (*go)->safeGetFloat("stickiness",  tradeGood->stickiness);
    tradeGood->decay       = (*go)->safeGetFloat("decay",       tradeGood->decay);
    tradeGood->consumption = (*go)->safeGetFloat("consumption", tradeGood->consumption);
    tradeGood->capital     = (*go)->safeGetFloat("capital",     tradeGood->capital);
  }

  TradeGood* laborForHardcodedValues = (TradeGood*) TradeGood::Labor;
  laborForHardcodedValues->decay       = 1.0;
  laborForHardcodedValues->consumption = 1.0;
  laborForHardcodedValues->capital     = 1.0;
}

void StaticInitialiser::initialiseGraphics (Object* gInfo) {
  Logger::logStream(DebugStartup) << "Entering StaticInitialiser::initialiseGraphics\n"; 

  // Must come after buildMilUnits so templates can check for icons. 
  Object* guiInfo = processFile("./common/gui.txt");
  if (!guiInfo) guiInfo = new Object("guiInfo");
  setUItexts(guiInfo); 
  
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

  Object* unitFormations = gInfo->getNeededObject("unitformations");
  MilUnitGraphicsInfo::allFormations.resize(unitFormations->safeGetInt("maxSprites", 9)+1);  
  for (unsigned int i = 1; i < MilUnitGraphicsInfo::allFormations.size(); ++i) {
    sprintf(strbuffer, "%i", i);
    Object* formation = unitFormations->safeGetObject(strbuffer);
    if (!formation) {
      for (unsigned int j = 0; j < i; ++j) {
	MilUnitGraphicsInfo::allFormations[i].push_back(doublet(0 - ((j+1)/2)*0.1, 0 + (((j+1)/2)*0.1)*(-1 + 2*(j%2)))); 
      }
    }
    else {
      objvec positions = formation->getValue("position");
      for (unsigned int j = 0; j < i; ++j) {
	if (j >= positions.size()) {
	  MilUnitGraphicsInfo::allFormations[i].push_back(doublet(0 - ((j+1)/2)*0.1, 0 + (((j+1)/2)*0.1)*(-1 + 2*(j%2))));
	}
	else {
	  MilUnitGraphicsInfo::allFormations[i].push_back(doublet(positions[j]->safeGetFloat("x", 0 - ((j+1)/2)*0.1),
								  positions[j]->safeGetFloat("y", 0 + (((j+1)/2)*0.1)*(-1 + 2*(j%2))))); 
	}
      }      
    }
  }
  Logger::logStream(DebugStartup) << "Leaving StaticInitialiser::initialiseGraphics\n";   
}

void StaticInitialiser::initialiseMarket (Market* market, Object* pInfo) {
  /*
  for (TradeGood::Iter tg = TradeGood::exMoneyStart(); tg != TradeGood::final(); ++tg) {
    market->prices[**tg] = pInfo->safeGetFloat((*tg)->getName(), 1);
  }
  */
}

void StaticInitialiser::initialiseMaslowHierarchy (Object* popNeeds) {
  assert(popNeeds);
  objvec levels = popNeeds->getValue("level");

  Village::maslowLevels.clear();
  int counter = 1;
  for (objiter level = levels.begin(); level != levels.end(); ++level) {
    Village::MaslowLevel* current = new Village::MaslowLevel();
    readGoodsHolder((*level), *current);
    current->mortalityModifier = (*level)->safeGetFloat("mortality", 1.0);
    current->maxWorkFraction = (*level)->safeGetFloat("max_work_fraction", 1.0);
    sprintf(strbuffer, "\"Goods level %i\"", counter++);
    current->name = remQuotes((*level)->safeGetString("name", strbuffer));
    Village::maslowLevels.push_back(current);
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
  Direction dir = getDirection(pos);
  if (NoDirection == dir) return 0;
  assert(hex->getLine(dir)); 
  return hex->getLine(dir); 
}

Vertex* findVertex (Object* info, Hex* hex) {
  string pos = info->safeGetString("vtx", "nowhere");
  if (pos == "nowhere") return 0;   
  Vertices dir = getVertex(pos); 
  if (NoVertex == dir) return 0; 
  assert(hex->getVertex(dir)); 
  return hex->getVertex(dir); 
}

void StaticInitialiser::initialiseBuilding (Building* build, Object* info) {
  build->supplies = info->safeGetFloat("supplies", 0);
  build->marginFactor = info->safeGetFloat("marginalDecline", build->marginFactor);
}

void StaticInitialiser::writeBuilding (Object* bInfo, Building* build) {
  bInfo->setLeaf("supplies", build->supplies);
  bInfo->setLeaf("marginalDecline", build->marginFactor);
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
    hex->castle = castle; 
    castle->setOwner(owner);
    initialiseBuilding(castle, cinfo);
    castle->recruitType = MilUnitTemplate::getUnitType(cinfo->safeGetString("recruiting", *(MilUnitTemplate::beginTypeNames()))); 
    Object* garrison = cinfo->safeGetObject("garrison");
    if (garrison) {
      MilUnit* m = buildMilUnit(garrison); 
      m->setOwner(owner);
      castle->addGarrison(m);
    }
    lin->addCastle(castle);
    initialiseEcon(castle, cinfo);
  }

  Object* priceInfo = hInfo->safeGetObject("prices");
  readGoodsHolder(priceInfo, hex->prices);
  
  Object* fInfo = hInfo->safeGetObject("village");
  if (fInfo) {
    Village* village = buildVillage(fInfo);
    initialiseBuilding(village, fInfo);
    if (owner) village->setOwner(owner);
    hex->setVillage(village);
  }

  fInfo = hInfo->safeGetObject("farmland");
  if (fInfo) {
    Farmland* farms = buildFarm(fInfo);
    initialiseBuilding(farms, fInfo);
    if (owner) farms->setOwner(owner);
    hex->setFarm(farms);
  }

  fInfo = hInfo->safeGetObject("forest");
  if (fInfo) {
    Forest* forest = buildForest(fInfo);
    initialiseBuilding(forest, fInfo);
    if (owner) forest->setOwner(owner);
    hex->setForest(forest);
  }

  fInfo = hInfo->safeGetObject("mine");

  if (fInfo) {
    Mine* mine = buildMine(fInfo);
    initialiseBuilding(mine, fInfo);
    if (owner) mine->setOwner(owner);
    hex->setMine(mine);
  }
  
  //initialiseMarket(hex, hInfo->getNeededObject("prices"));
}

ThreeDSprite* makeSprite (Object* info) {
  string castleFile = info->safeGetString("filename", "nosuchbeast");
  assert(castleFile != "nosuchbeast");
  vector<string> specs;
  objvec svec = info->getValue("separate");
  for (objiter s = svec.begin(); s != svec.end(); ++s) {
    specs.push_back((*s)->getLeaf()); 
  }
  ThreeDSprite* ret = new ThreeDSprite(castleFile, specs);
  ret->setScale(info->safeGetFloat("xscale", 1.0), info->safeGetFloat("yscale", 1.0), info->safeGetFloat("zscale", 1.0)); 
  return ret; 
}


void readAgeTrackerFromObject (AgeTracker& age, Object* obj) {
  if (!obj) return; 
  for (int i = 0; i < AgeTracker::maxAge; ++i) {
    if (i >= obj->numTokens()) continue;
    age.addPop(obj->tokenAsInt(i), i);
  }
}

Farmland* StaticInitialiser::buildFarm (Object* fInfo) {
  Farmland* ret = new Farmland();
  initialiseCollective<Farmer, FieldStatus, Farmland::numOwners>(ret, fInfo);
  ret->countTotals();
  ret->blockSize = fInfo->safeGetInt("blockSize", ret->blockSize);
  
  return ret;
}


Forest* StaticInitialiser::buildForest (Object* fInfo) {
  Forest* ret = new Forest();
  /*
  Object* wild    = fInfo->safeGetObject("wild");
  Object* clear   = fInfo->safeGetObject("cleared");
  Object* plant   = fInfo->safeGetObject("planted");
  Object* scrub   = fInfo->safeGetObject("scrubby");
  Object* sapling = fInfo->safeGetObject("sapling");
  Object* young   = fInfo->safeGetObject("young");
  Object* grown   = fInfo->safeGetObject("grown");
  Object* mature  = fInfo->safeGetObject("mature");
  Object* mighty  = fInfo->safeGetObject("mighty");
  Object* huge    = fInfo->safeGetObject("huge");
  Object* climax  = fInfo->safeGetObject("climax");
  Object* owner   = fInfo->safeGetObject("owner");

  objvec workers = fInfo->getValue("worker");
  if ((int) workers.size() < Forest::numOwners) throwFormatted("Expected %i worker objects, found %i", Forest::numOwners, workers.size());
  for (int i = 0; i < Forest::numOwners; ++i) {
    initialiseEcon(ret->foresters[i], workers[i]);
    ret->foresters[i]->groves[*ForestStatus::Wild]     = wild    ? (wild->numTokens()    > i ? wild->tokenAsInt(i)    : 0) : 0;
    ret->foresters[i]->groves[*ForestStatus::Clear]    = clear   ? (clear->numTokens()   > i ? clear->tokenAsInt(i)   : 0) : 0;
    ret->foresters[i]->groves[*ForestStatus::Planted]  = plant   ? (plant->numTokens()   > i ? plant->tokenAsInt(i)   : 0) : 0;
    ret->foresters[i]->groves[*ForestStatus::Scrub]    = scrub   ? (scrub->numTokens()   > i ? scrub->tokenAsInt(i)   : 0) : 0;
    ret->foresters[i]->groves[*ForestStatus::Saplings] = sapling ? (sapling->numTokens() > i ? sapling->tokenAsInt(i) : 0) : 0;
    ret->foresters[i]->groves[*ForestStatus::Young]    = young   ? (young->numTokens()   > i ? young->tokenAsInt(i)   : 0) : 0;
    ret->foresters[i]->groves[*ForestStatus::Grown]    = grown   ? (grown->numTokens()   > i ? grown->tokenAsInt(i)   : 0) : 0;
    ret->foresters[i]->groves[*ForestStatus::Mature]   = mature  ? (mature->numTokens()  > i ? mature->tokenAsInt(i)  : 0) : 0;
    ret->foresters[i]->groves[*ForestStatus::Mighty]   = mighty  ? (mighty->numTokens()  > i ? mighty->tokenAsInt(i)  : 0) : 0;
    ret->foresters[i]->groves[*ForestStatus::Huge]     = huge    ? (huge->numTokens()    > i ? huge->tokenAsInt(i)    : 0) : 0;
    ret->foresters[i]->groves[*ForestStatus::Climax]   = climax  ? (climax->numTokens()  > i ? climax->tokenAsInt(i)  : 0) : 0;
    ret->foresters[i]->owner                    = owner   ? (owner->numTokens()   > i ? EconActor::getByIndex(owner->tokenAsInt(i)) : 0) : 0;
    ret->foresters[i]->createBlockQueue();
  }
  */
  initialiseCollective<Forester, ForestStatus, Forest::numOwners>(ret, fInfo);
  ret->yearsSinceLastTick = fInfo->safeGetInt("yearsSinceLastTick");
  ret->blockSize = fInfo->safeGetInt("blockSize", ret->blockSize);
  ret->minStatusToHarvest = ForestStatus::Huge;
  
  return ret;
}

Mine* StaticInitialiser::buildMine (Object* mInfo) {
  Mine* ret = new Mine();

  for (Mine::MineStatus::Iter ms = Mine::MineStatus::start(); ms != Mine::MineStatus::final(); ++ms) {
    Object* status = mInfo->getNeededObject((*ms)->getName());
    for (int i = 0; i < Mine::numOwners; ++i) {  
      ret->miners[i]->shafts[**ms] = (status->numTokens() > i ? status->tokenAsInt(i) : 0);
    }
  }
  Object* owner = mInfo->getNeededObject("owner");
  objvec workers = mInfo->getValue("worker");
  if ((int) workers.size() < Mine::numOwners) throwFormatted("Expected %i worker objects, found %i", Mine::numOwners, workers.size());
  for (int i = 0; i < Mine::numOwners; ++i) {
    initialiseEcon(ret->miners[i], workers[i]);
    ret->miners[i]->owner = (owner->numTokens() > i ? EconActor::getByIndex(owner->tokenAsInt(i)) : 0);
  }

  ret->veinsPerMiner = mInfo->safeGetInt("veinsPerShaft", ret->veinsPerMiner);
  return ret;
}

void StaticInitialiser::buildMilitia (Village* target, Object* mInfo) {
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

  initialiseEcon(m, mInfo);
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
   
    if (nType->militiaDrill > mDrill) nType->militiaDrill = mDrill; 

    Object* spriteInfo = (*unit)->safeGetObject("sprite");
    if (spriteInfo) {
      milUnitSpriteObjectMap[nType] = spriteInfo;
      /*
      ThreeDSprite* nSprite = makeSprite(spriteInfo);
      MilUnitGraphicsInfo::indexMap[nType] = SpriteContainer::sprites.size();
      MilUnitSprite* mSprite = new MilUnitSprite();
      mSprite->soldier = nSprite;
      objvec positions = spriteInfo->getValue("position");
      if (0 == positions.size()) mSprite->positions.push_back(doublet(0, 0));
      else {
	for (objiter p = positions.begin(); p != positions.end(); ++p) {
	  mSprite->positions.push_back(doublet((*p)->safeGetFloat("x"), (*p)->safeGetFloat("y"))); 
	}
      }
      SpriteContainer::sprites.push_back(mSprite);
      */      
    }
  }
}

Village* StaticInitialiser::buildVillage (Object* fInfo) {
  Village* ret = new Village();
  Object* males = fInfo->safeGetObject("males");
  Object* females = fInfo->safeGetObject("females");
  readAgeTrackerFromObject(ret->males, males);
  readAgeTrackerFromObject(ret->women, females);
  ret->updateMaxPop();

  buildMilitia(ret, fInfo->safeGetObject("militiaUnits"));
  initialiseEcon(ret, fInfo);
  return ret;
}

void StaticInitialiser::createPlayer (Object* info) {
  static int numFactions = 0;
  
  bool human = (info->safeGetString("human", "no") == "yes");
  sprintf(strbuffer, "\"faction_auto_name_%i\"", numFactions++); 
  string name = info->safeGetString("name", strbuffer);
  string display = remQuotes(info->safeGetString("displayname", name));
  Player* ret = new Player(human, display, name);
  initialiseEcon(ret, info); 
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

void StaticInitialiser::addShadows (QGLFramebufferObject* fbo, GLuint texture) {
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

void createTexture (QGLFramebufferObject* fbo, int minHeight, int maxHeight, double* heightMap, int mapWidth, GLuint texture) {
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

void StaticInitialiser::loadSprites () {
  Logger::logStream(DebugStartup) << "loadSprites begin\n";
  for (map<MilUnitTemplate*, Object*>::iterator muto = milUnitSpriteObjectMap.begin(); muto != milUnitSpriteObjectMap.end(); ++muto) {
    Logger::logStream(DebugStartup) << (*muto).first->name << "\n";
    Object* spriteInfo = (*muto).second;
    ThreeDSprite* nSprite = makeSprite(spriteInfo);
    MilUnitGraphicsInfo::indexMap[(*muto).first] = SpriteContainer::sprites.size();
    MilUnitSprite* mSprite = new MilUnitSprite();
    mSprite->soldier = nSprite;
    objvec positions = spriteInfo->getValue("position");
    if (0 == positions.size()) mSprite->positions.push_back(doublet(0, 0));
    else {
      for (objiter p = positions.begin(); p != positions.end(); ++p) {
	mSprite->positions.push_back(doublet((*p)->safeGetFloat("x"), (*p)->safeGetFloat("y"))); 
      }
    }
    SpriteContainer::sprites.push_back(mSprite);
  }
  
  if (WarfareWindow::currWindow->hexDrawer->cSprite) delete WarfareWindow::currWindow->hexDrawer->cSprite;
  if (WarfareWindow::currWindow->hexDrawer->tSprite) delete WarfareWindow::currWindow->hexDrawer->tSprite;
  if (WarfareWindow::currWindow->hexDrawer->farmSprite) delete WarfareWindow::currWindow->hexDrawer->farmSprite;  
  
  Object* ginfo = processFile("gfx/info.txt");

  Object* spriteInfo = ginfo->safeGetObject("castlesprite");
  assert(spriteInfo);
  WarfareWindow::currWindow->hexDrawer->cSprite = makeSprite(spriteInfo);
  
  spriteInfo = ginfo->safeGetObject("treesprite");
  assert(spriteInfo);
  WarfareWindow::currWindow->hexDrawer->tSprite = makeSprite(spriteInfo);

  spriteInfo = ginfo->safeGetObject("farmsprite");
  assert(spriteInfo);  
  WarfareWindow::currWindow->hexDrawer->farmSprite = makeSprite(spriteInfo);

  spriteInfo = ginfo->safeGetObject("supplysprite");
  assert(spriteInfo);
  VillageGraphicsInfo::supplySpriteIndex = SpriteContainer::sprites.size();
  ThreeDSprite* cow = makeSprite(spriteInfo);
  MilUnitSprite* mSprite = new MilUnitSprite();
  mSprite->soldier = cow; 
  mSprite->positions.push_back(doublet(0, 0));
  SpriteContainer::sprites.push_back(mSprite);
  Object* pos = ginfo->getNeededObject("cowPositions");
  objvec cows = pos->getValue("position");
  VillageGraphicsInfo::maxCows = pos->safeGetInt("maxCows", 15);
  double maxSupplies = VillageGraphicsInfo::maxCows;
  for (VillageGraphicsInfo::Iterator f = VillageGraphicsInfo::begin(); f != VillageGraphicsInfo::end(); ++f) {
    maxSupplies = max(maxSupplies, (*f)->myVillage->getAvailableSupplies());
  }
  double tolerance = pos->safeGetFloat("cowTolerance", 1.5);
  VillageGraphicsInfo::suppliesPerCow = (int) floor(maxSupplies*tolerance/VillageGraphicsInfo::maxCows); 
  
  for (int i = 0; i < VillageGraphicsInfo::maxCows; ++i) {
    if (i >= (int) cows.size()) {
      VillageGraphicsInfo::cowPositions.push_back(doublet((i%3)*0.1, (i/3)*0.1));
    }
    else {
      VillageGraphicsInfo::cowPositions.push_back(doublet(cows[i]->safeGetFloat("x", (i%3)*0.1),
							  cows[i]->safeGetFloat("y", (i/3)*0.1)));
    }
  }    
  milUnitSpriteObjectMap.clear();
}

void StaticInitialiser::loadTextures () {
  FarmGraphicsInfo::textureIndices.push_back(loadTexture("gfx\\cleared.png", Qt::darkGray));
  FarmGraphicsInfo::textureIndices.push_back(loadTexture("gfx\\ploughed.png", Qt::black));
  FarmGraphicsInfo::textureIndices.push_back(loadTexture("gfx\\sowed.png", Qt::gray));
  FarmGraphicsInfo::textureIndices.push_back(loadTexture("gfx\\ripe1.png", Qt::darkGreen));
  FarmGraphicsInfo::textureIndices.push_back(loadTexture("gfx\\ripe2.png", Qt::green));
  FarmGraphicsInfo::textureIndices.push_back(loadTexture("gfx\\ripe3.png", Qt::yellow));
  FarmGraphicsInfo::textureIndices.push_back(loadTexture("gfx\\reaped.png", Qt::darkYellow));
}

void StaticInitialiser::makeGraphicsInfoObjects () {
  for (Hex::Iterator h = Hex::start(); h != Hex::final(); ++h) {
    (*h)->graphicsInfo = new HexGraphicsInfo(*h);
    for (int i = LeftUp; i < NoVertex; ++i) {
      Vertex* vex = (*h)->vertices[i];
      if (vex->graphicsInfo) continue;
      vex->graphicsInfo = new VertexGraphicsInfo(vex, (*h)->getGraphicsInfo(), convertToVertex(i));
    }
    if ((*h)->village) (*h)->setGraphicsVillage((*h)->village);
    if ((*h)->farms) (*h)->setGraphicsFarm((*h)->farms); 
  }
  for (Line::Iterator l = Line::start(); l != Line::final(); ++l) {
    (*l)->graphicsInfo = new LineGraphicsInfo((*l), (*l)->vex1->getDirection((*l)->vex2));
    if ((*l)->getCastle()) (*l)->addGraphicCastle((*l)->getCastle()); 
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
  hexDrawer->terrainTextureIndices = new GLuint[terrains.size()];
  hexDrawer->zoneTextures = new GLuint[1];
  for (unsigned int i = 0; i < terrains.size(); ++i) {
    hexDrawer->terrainTextureIndices[i] = loadTexture(remQuotes(terrains[i]->safeGetString("file")), Qt::blue);
    int minHeight = terrains[i]->safeGetInt("minimum");
    int maxHeight = terrains[i]->safeGetInt("maximum");    
    createTexture(fbo, minHeight, maxHeight, GraphicsInfo::heightMap, heightMapWidth(GraphicsInfo::zoneSide), hexDrawer->terrainTextureIndices[i]);   
  }

  GLuint shadowTexture = 0;
  glGenTextures(1, &shadowTexture); 
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
}

void StaticInitialiser::setUItexts (Object* tInfo) {
  static const string badString("UNKNOWN STRING WANTED");
  Object* icons = tInfo->getNeededObject("icons");
  
  WarfareWindow::currWindow->plainMapModeButton.setToolTip(remQuotes(tInfo->safeGetString("plainMapMode", badString)).c_str());
  string iconfile = icons->safeGetString("plain", badString);
  if ((iconfile != badString) && (QFile::exists(iconfile.c_str()))) WarfareWindow::currWindow->plainMapModeButton.setIcon(QIcon(iconfile.c_str()));

  WarfareWindow::currWindow->supplyMapModeButton.setToolTip(remQuotes(tInfo->safeGetString("supplyMapMode", badString)).c_str());
  iconfile = icons->safeGetString("supply", badString);
  if ((iconfile != badString) && (QFile::exists(iconfile.c_str()))) WarfareWindow::currWindow->supplyMapModeButton.setIcon(QIcon(iconfile.c_str()));  

  WarfareWindow::currWindow->unitInterface->increasePriorityButton.setToolTip(remQuotes(tInfo->safeGetString("incPrior", badString)).c_str());
  iconfile = icons->safeGetString("incPriority", badString);
  if ((iconfile != badString) && (QFile::exists(iconfile.c_str()))) {
    WarfareWindow::currWindow->unitInterface->increasePriorityButton.setArrowType(Qt::NoArrow);
    WarfareWindow::currWindow->unitInterface->increasePriorityButton.setIcon(QIcon(iconfile.c_str()));
  }
  WarfareWindow::currWindow->unitInterface->decreasePriorityButton.setToolTip(remQuotes(tInfo->safeGetString("decPrior", badString)).c_str());
  iconfile = icons->safeGetString("decPriority", badString);
  if ((iconfile != badString) && (QFile::exists(iconfile.c_str()))) {
    WarfareWindow::currWindow->unitInterface->decreasePriorityButton.setArrowType(Qt::NoArrow);
    WarfareWindow::currWindow->unitInterface->decreasePriorityButton.setIcon(QIcon(iconfile.c_str()));
  }
 
  WarfareWindow::currWindow->castleInterface->increaseRecruitButton.setToolTip(remQuotes(tInfo->safeGetString("incRecruit", badString)).c_str());
  WarfareWindow::currWindow->castleInterface->decreaseRecruitButton.setToolTip(remQuotes(tInfo->safeGetString("decRecruit", badString)).c_str());
  for (MilUnitTemplate::Iterator unit = MilUnitTemplate::begin(); unit != MilUnitTemplate::end(); ++unit) {
    iconfile = icons->safeGetString((*unit)->name, badString);
    Logger::logStream(DebugStartup) << "Setting icon " << iconfile << "\n"; 
    if ((iconfile != badString) && (QFile::exists(iconfile.c_str()))) CastleInterface::icons[*unit] = QIcon(iconfile.c_str()); 
  }
  
  WarfareWindow::currWindow->villageInterface->increaseDrillButton.setToolTip(remQuotes(tInfo->safeGetString("incDrill", badString)).c_str());
  iconfile = icons->safeGetString("incDrill", badString);
  if ((iconfile != badString) && (QFile::exists(iconfile.c_str()))) {
    WarfareWindow::currWindow->villageInterface->increaseDrillButton.setArrowType(Qt::NoArrow);
    WarfareWindow::currWindow->villageInterface->increaseDrillButton.setIcon(QIcon(iconfile.c_str()));
  }  
  WarfareWindow::currWindow->villageInterface->decreaseDrillButton.setToolTip(remQuotes(tInfo->safeGetString("decDrill", badString)).c_str());
  iconfile = icons->safeGetString("decDrill", badString);
  if ((iconfile != badString) && (QFile::exists(iconfile.c_str()))) {
    WarfareWindow::currWindow->villageInterface->decreaseDrillButton.setArrowType(Qt::NoArrow);
    WarfareWindow::currWindow->villageInterface->decreaseDrillButton.setIcon(QIcon(iconfile.c_str()));
  }
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
  writeEconActorIntoObject(unit, obj);
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

void StaticInitialiser::writeEconActorIntoObject (EconActor* econ, Object* info) {
  info->setLeaf("id", econ->getIdx());
  writeGoodsHolderIntoObject(*econ, info->getNeededObject("goods"));
  if (econ->getEconOwner()) info->setLeaf("owner", econ->getEconOwner()->getIdx());
}

void StaticInitialiser::writeGoodsHolderIntoObject (const GoodsHolder& goodsHolder, Object* info) {
  for (TradeGood::Iter tg = TradeGood::start(); tg != TradeGood::final(); ++tg) {
    double amount = goodsHolder.getAmount(*tg);
    if (fabs(amount) < 0.001) continue;
    string gname = (*tg)->getName();
    info->setLeaf(gname, amount); 
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
  
  for (Player::Iter p = Player::start(); p != Player::final(); ++p) {
    Object* faction = new Object("faction");
    faction->setLeaf("name", (*p)->getName());
    faction->setLeaf("displayname", string("\"") + (*p)->getDisplayName() + "\"");
    faction->setLeaf("human", (*p)->isHuman() ? "yes" : "no");
    PlayerGraphicsInfo const* pgInfo = (*p)->getGraphicsInfo();
    faction->setLeaf("red",   pgInfo->getRed());
    faction->setLeaf("green", pgInfo->getGreen());
    faction->setLeaf("blue",  pgInfo->getBlue());
    writeEconActorIntoObject((*p), faction);
    
    game->setValue(faction); 
  }

  game->setLeaf("currentplayer", Player::getCurrentPlayer()->getName()); 

  int maxx = -1;
  int maxy = -1;
  for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) {
    maxx = max((*hex)->getPos().first, maxx);
    maxy = max((*hex)->getPos().second, maxy);
  }

  Object* hexgrid = new Object("hexgrid");
  hexgrid->setLeaf("x", maxx+1);
  hexgrid->setLeaf("y", maxy+1);
  game->setValue(hexgrid); 

  for (Hex::Iterator hex = Hex::start(); hex != Hex::final(); ++hex) {
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
      writeEconActorIntoObject(castle, castleObject); 
      castleObject->setLeaf("pos", getDirectionName((*hex)->getDirection(*lin)));
      castleObject->setLeaf("supplies", castle->supplies);
      castleObject->setLeaf("recruiting", castle->recruitType->name); 
      for (unsigned int i = 0; (int) i < castle->numGarrison(); ++i) {
	Object* garrObject = new Object("garrison");
	castleObject->setValue(garrObject);
	writeUnitToObject(castle->getGarrison(i), garrObject);
	writeEconActorIntoObject(castle->getGarrison(i), garrObject); 
      }
      /*
      Object* taxObject = new Object("taxes");
      castleObject->setValue(taxObject);
      taxObject->setLeaf("amount", castle->taxExtraction.amount);
      switch (castle->taxExtraction.delivery) {
      default: 
      case ContractInfo::Fixed:             taxObject->setLeaf("type", "fixed"); break;
      case ContractInfo::Percentage:        taxObject->setLeaf("type", "percentage"); break;
      case ContractInfo::SurplusPercentage: taxObject->setLeaf("type", "surplus_percentage"); break;
      }
      */
    }

    writeGoodsHolderIntoObject((*hex)->prices, hexInfo->getNeededObject("prices"));

    Village* village = (*hex)->getVillage();
    if (village) {
      Object* villageInfo = new Object("village");
      hexInfo->setValue(villageInfo);
      writeEconActorIntoObject(village, villageInfo); 
      Object* males = new Object("males");
      writeAgeInfoToObject(village->males, males);
      villageInfo->setValue(males);

      males = new Object("females");      
      writeAgeInfoToObject(village->women, males);
      villageInfo->setValue(males);

      villageInfo->setLeaf("supplies", village->supplies);
    }
    
    Farmland* farm = (*hex)->getFarm();
    if (farm) {
      Object* farmInfo = new Object("farmland");
      writeBuilding(farmInfo, farm);
      hexInfo->setValue(farmInfo);
      writeCollective<Farmer, FieldStatus, Farmland::numOwners>(farm, farmInfo);
      farmInfo->setLeaf("blockSize", farm->blockSize);
    }

    Forest* forest = (*hex)->forest;
    if (forest) {
      Object* forestInfo = new Object("forest");
      writeBuilding(forestInfo, forest);
      hexInfo->setValue(forestInfo);
      writeCollective<Forester, ForestStatus, Forest::numOwners>(forest, forestInfo);
      Object* tended = new Object("tended");
      tended->setObjList(true);
      forestInfo->setValue(tended);
      for (int i = 0; i < Forest::numOwners; ++i) {
	tended->addToList((int) forest->workers[i]->tendedGroves);
      }
      forestInfo->setLeaf("yearsSinceLastTick", forest->yearsSinceLastTick);
      forestInfo->setLeaf("blockSize", forest->blockSize);
      forestInfo->setLeaf("minStatusToHarvest", *ForestStatus::Huge);
    }

    Mine* mine = (*hex)->mine;
    if (mine) {
      Object* mineInfo = new Object("mine");
      writeBuilding(mineInfo, mine);
      hexInfo->setValue(mineInfo);
      for (int i = 0; i < Mine::numOwners; ++i) {
	Object* worker = new Object("worker");
	mineInfo->setValue(worker);
	writeEconActorIntoObject(mine->miners[i], worker);
      }

      for (Mine::MineStatus::Iter ms = Mine::MineStatus::start(); ms != Mine::MineStatus::final(); ++ms) {
	Object* statusInfo = new Object((*ms)->getName());
	mineInfo->setValue(statusInfo);
	statusInfo->setObjList(true);
	for (int i = 0; i < Mine::numOwners; ++i) {
	  statusInfo->addToList(mine->miners[i]->shafts[**ms]);
	}
      }
      Object* owner = new Object("owner");
      mineInfo->setValue(owner);
      owner->setObjList(true);
      for (int i = 0; i < Mine::numOwners; ++i) {
	owner->addToList((int) mine->miners[i]->owner->getIdx());
      }
      mineInfo->setLeaf("veinsPerShaft", mine->veinsPerMiner);
    }
  }
  
  for (Vertex::Iterator vtx = Vertex::start(); vtx != Vertex::final(); ++vtx) {
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
    uinfo->setLeaf("vtx", getVertexName(hex->getDirection(*vtx)));
    writeUnitToObject(unit, uinfo); 
    game->setValue(uinfo);
  }
 
  ofstream writer;
  writer.open(fname.c_str());
  writer << (*game) << std::endl;
  writer.close(); 
}
