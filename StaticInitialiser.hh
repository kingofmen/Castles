#ifndef STATIC_INITALISER
#define STATIC_INITALISER

class AgeTracker;
class EconActor;
class Farmland;
class Forest;
class GLDrawer;
class Market;
class MilUnit;
class MilUnitTemplate;
class Object;
class TradeUnit;
class TransportUnit;
class Unit;
class Village;

#include "game/Action.hh"

class StaticInitialiser {
  // In effect a namespace, for all those functions that get called once at startup. 
public:
  static void      overallInitialisation (Object* info); 
  static void      graphicsInitialisation ();  
  static void      initialiseBuilding (Building* build, Object* info); 
  static void      initialiseCivilBuildings (Object* popInfo);
  static void      initialiseEcon (EconActor* econ, Object* info); 
  static void      initialiseGoods (Object* gInfo); 
  static void      initialiseGraphics (Object* gInfo);
  static void      initialiseMaslowHierarchy (Object* popNeeds); 
  static void      buildHex (Object* hInfo);
  static void      buildMilitia (Village* target, Object* mInfo);  
  static MilUnit*  buildMilUnit (Object* mInfo);
  static void      buildMilUnitTemplates (Object* info);
  static void      buildTradeUnit (Object* info);
  static void      buildTransportUnit (Object* info);
  static Village*  buildVillage (Object* fInfo);
  static void      clearTempMaps ();
  static void      createActionProbabilities (Object* info);
  static void      createPlayer (Object* info);   
  static void      loadAiConstants (Object* info);
  static void      loadSprites (); 
  static void      loadTextures ();
  static void      makeGraphicsInfoObjects ();   
  static void      makeZoneTextures (Object* gInfo);
  static void      setUItexts (Object* tInfo);
  static void      unitTests();
  
  static void      writeGameToFile (string fname);
  static void      writeAgeInfoToObject (AgeTracker& age, Object* obj, int skip = 0);  
  static void      writeUnitToObject (MilUnit* unit, Object* obj);
  static void      writeTradeUnitToObject (TradeUnit* unit, Object* obj);
  static void      writeTransportUnitToObject (TransportUnit* unit, Object* obj);
  
private:
  template<class W> static void readInts (W* target, Object* source, map<string, int W::*> memMap) {
    for (typename map<string, int W::*>::iterator i = memMap.begin(); i != memMap.end(); ++i) {
      target->*((*i).second) = source->safeGetInt((*i).first, target->*((*i).second));
    }
  }

  template<class W> static void readFloats (W* target, Object* source, map<string, double W::*> memMap) {
    for (typename map<string, double W::*>::iterator i = memMap.begin(); i != memMap.end(); ++i) {
      target->*((*i).second) = source->safeGetFloat((*i).first, target->*((*i).second));
    }
  }

  template<class W> static void writeInts (W* source, Object* target, map<string, int W::*> memMap) {
    for (typename map<string, int W::*>::iterator i = memMap.begin(); i != memMap.end(); ++i) {
      target->setLeaf((*i).first, source->*((*i).second));
    }
  }

  template<class W> static void writeFloats (W* source, Object* target, map<string, double W::*> memMap) {
    for (typename map<string, double W::*>::iterator i = memMap.begin(); i != memMap.end(); ++i) {
      target->setLeaf((*i).first, source->*((*i).second));
    }
  }

  template <class C> static void initialiseCollective (C* collective,
						       Object* cInfo,
						       map<string, int C::WorkerType::*> intMap,
						       map<string, double C::WorkerType::*> floatMap) {
    objvec wInfos = cInfo->getValue("worker");
    if ((int) wInfos.size() < C::numOwners) throwFormatted("Expected %i worker objects, found %i", C::numOwners, wInfos.size());
    for (int i = 0; i < C::numOwners; ++i) {
      initialiseEcon(collective->workers[i], wInfos[i]);
      for (typename C::StatusType::Iter stat = C::StatusType::start(); stat != C::StatusType::final(); ++stat) {
	collective->workers[i]->fields[**stat] = wInfos[i]->safeGetInt((*stat)->getName(), 0);
      }
      readInts<typename C::WorkerType>(collective->workers[i], wInfos[i], intMap);
      readFloats<typename C::WorkerType>(collective->workers[i], wInfos[i], floatMap);
    }
  }

  template <class C> static void initialiseCollective (C* collective, Object* cInfo, map<string, int C::WorkerType::*> intMap) {
    initialiseCollective<C>(collective, cInfo, intMap, map<string, double C::WorkerType::*>());
  }

  template <class C> static void writeCollective (C* collective,
						  Object* cInfo,
						  map<string, int C::WorkerType::*> intMap,
						  map<string, double C::WorkerType::*> floatMap) {
    for (int i = 0; i < C::numOwners; ++i) {
      Object* worker = new Object("worker");
      cInfo->setValue(worker);
      writeEconActorIntoObject(collective->workers[i], worker);
      for (typename C::StatusType::Iter fs = C::StatusType::start(); fs != C::StatusType::final(); ++fs) {
	worker->setLeaf((*fs)->getName(), collective->workers[i]->fields[**fs]);
      }
      writeInts<typename C::WorkerType>(collective->workers[i], worker, intMap);
      writeFloats<typename C::WorkerType>(collective->workers[i], worker, floatMap);
    }
  }

  template <class C> static void writeCollective (C* collective, Object* cInfo, map<string, int C::WorkerType::*> intMap) {
    writeCollective<C>(collective, cInfo, intMap, map<string, double C::WorkerType::*>());
  }
  
  static Farmland* buildFarm (Object* fInfo);
  static Forest*   buildForest (Object* fInfo);
  static Mine*     buildMine (Object* mInfo);  
  template<class T> static void initialiseIndustry(Object* industryObject);
    
  static void addShadows (QGLFramebufferObject* fbo, GLuint texture); 
  static void createCalculator (Object* info, Action::Calculator* ret);
  static double interpolate (double xfrac, double yfrac, int width, int height, double* heightMap);
  static void setPlayer (Unit* unit, Object* mInfo);
  static void writeGoodsHolderIntoObject (const GoodsHolder& goodsHolder, Object* info);
  static void writeContractInfoIntoObject (MarketContract* contract, Object* info);
  static void writeEconActorIntoObject (EconActor* econ, Object* info);
  static void writeBuilding (Object* bInfo, Building* build);
  static void writeObligationInfoIntoObject (ContractInfo* contract, Object* info);
  static void writeUnitLocation (Unit* unit, Object* obj);
  static void writeVertex (Vertex* vtx, Object* obj);
  
  static int defaultUnitPriority;
}; 

#endif
