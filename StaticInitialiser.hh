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
  static void      initialiseMarket (Market* market, Object* pInfo);
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
  template <class W, class S, int N> static void initialiseCollective (Collective<W, S, N>* collective, Object* cInfo) {
    objvec wInfos = cInfo->getValue("worker");
    if ((int) wInfos.size() < N) throwFormatted("Expected %i worker objects, found %i", N, wInfos.size());
    for (int i = 0; i < N; ++i) {
      W* currWorker = collective->workers[i];
      initialiseEcon(currWorker, wInfos[i]);
      for (typename S::Iter stat = S::start(); stat != S::final(); ++stat) {
	currWorker->fields[**stat] = wInfos[i]->safeGetInt((*stat)->getName(), 0);
      }
    }
  }

  template <class W, class S, int N> static void writeCollective (Collective<W, S, N>* collective, Object* cInfo) {
    for (int i = 0; i < N; ++i) {
      Object* worker = new Object("worker");
      cInfo->setValue(worker);
      writeEconActorIntoObject(collective->workers[i], worker);
      for (typename S::Iter fs = S::start(); fs != S::final(); ++fs) {
	worker->setLeaf((*fs)->getName(), collective->workers[i]->fields[**fs]);
      }
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
