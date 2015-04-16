#ifndef STATIC_INITALISER
#define STATIC_INITALISER

class Object; 
class Farmland;
class Forest;
class Village; 
class MilUnit;
class MilUnitTemplate; 
class GLDrawer; 
class AgeTracker;
class EconActor; 
class Market;

#include "Action.hh" 

class StaticInitialiser {
public:

  // In effect a namespace, for all those functions that get called once at startup. 
  
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
  static Village*  buildVillage (Object* fInfo);
  static void      createActionProbabilities (Object* info);
  static void      createPlayer (Object* info);   
  static void      loadAiConstants (Object* info);
  static void      loadSprites (); 
  static void      loadTextures ();
  static void      makeGraphicsInfoObjects ();   
  static void      makeZoneTextures (Object* gInfo);
  static void      setUItexts (Object* tInfo);
  
  static void      writeGameToFile (string fname);
  static void      writeAgeInfoToObject (AgeTracker& age, Object* obj, int skip = 0);  
  static void      writeUnitToObject (MilUnit* unit, Object* obj);
  
private:
  template<class W> static void readInts (W* target, Object* source, map<string, int W::*> memMap) {
    for (typename map<string, int W::*>::iterator i = memMap.begin(); i != memMap.end(); ++i) {
      target->*((*i).second) = source->safeGetInt((*i).first);
    }
  }

  template<class W> static void writeInts (W* source, Object* target, map<string, int W::*> memMap) {
    for (typename map<string, int W::*>::iterator i = memMap.begin(); i != memMap.end(); ++i) {
      target->setLeaf((*i).first, source->*((*i).second));
    }
  }
  
  template <class C> static void initialiseCollective (C* collective, Object* cInfo, map<string, int C::WorkerType::*> intMap) {
    objvec wInfos = cInfo->getValue("worker");
    if ((int) wInfos.size() < C::numOwners) throwFormatted("Expected %i worker objects, found %i", C::numOwners, wInfos.size());
    C::createWorkers(collective);
    for (int i = 0; i < C::numOwners; ++i) {
      initialiseEcon(collective->workers[i], wInfos[i]);
      for (typename C::StatusType::Iter stat = C::StatusType::start(); stat != C::StatusType::final(); ++stat) {
	collective->workers[i]->fields[**stat] = wInfos[i]->safeGetInt((*stat)->getName(), 0);
      }
      readInts<typename C::WorkerType>(collective->workers[i], wInfos[i], intMap);
    }
  }

  template <class C> static void writeCollective (C* collective, Object* cInfo, map<string, int C::WorkerType::*> intMap) {
    for (int i = 0; i < C::numOwners; ++i) {
      Object* worker = new Object("worker");
      cInfo->setValue(worker);
      writeEconActorIntoObject(collective->workers[i], worker);
      for (typename C::StatusType::Iter fs = C::StatusType::start(); fs != C::StatusType::final(); ++fs) {
	worker->setLeaf((*fs)->getName(), collective->workers[i]->fields[**fs]);
      }
      writeInts<typename C::WorkerType>(collective->workers[i], worker, intMap);
    }
  }

  static Farmland* buildFarm (Object* fInfo);
  static Forest*   buildForest (Object* fInfo);
  static Mine*     buildMine (Object* mInfo);  
  template<class T> static void initialiseIndustry(Object* industryObject);
    
  static void addShadows (QGLFramebufferObject* fbo, GLuint texture); 
  static void createCalculator (Object* info, Action::Calculator* ret);
  static double interpolate (double xfrac, double yfrac, int width, int height, double* heightMap);
  static void writeGoodsHolderIntoObject (const GoodsHolder& goodsHolder, Object* info);
  static void writeEconActorIntoObject (EconActor* econ, Object* info);
  static void writeBuilding (Object* bInfo, Building* build);
  
  static int defaultUnitPriority;
}; 

#endif
