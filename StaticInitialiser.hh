#ifndef STATIC_INITALISER
#define STATIC_INITALISER

class Object; 
class Farmland;
class Village; 
class MilUnit;
class MilUnitTemplate; 
class GLDrawer; 
class AgeTracker;
class EconActor; 

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
  static Farmland* buildFarm (Object* fInfo);
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
  static void      makeZoneTextures (Object* gInfo); 
  static void      setUItexts (Object* tInfo);
  
  static void      writeGameToFile (string fname);
  static void      writeAgeInfoToObject (AgeTracker& age, Object* obj, int skip = 0);  
  static void      writeUnitToObject (MilUnit* unit, Object* obj);

  
private:
  static void addShadows (QGLFramebufferObject* fbo, GLuint texture); 
  static void createCalculator (Object* info, Action::Calculator* ret);
  static double interpolate (double xfrac, double yfrac, int width, int height, double* heightMap); 
  
  static int defaultUnitPriority;
}; 

#endif
