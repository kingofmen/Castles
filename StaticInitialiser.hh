#ifndef STATIC_INITALISER
#define STATIC_INITALISER

class Object; 
class Farmland;
class CivilBuilding; 
class MilUnit;
class MilUnitTemplate; 
class GLDrawer; 
class AgeTracker; 

#include "Action.hh" 

class StaticInitialiser {
public:

  // In effect a namespace, for all those functions that get called once at startup. 
  
  static void      overallInitialisation (Object* info); 
  static void      graphicsInitialisation ();  
  static void      initialiseBuilding (Building* build, Object* info); 
  static void      initialiseCivilBuildings (Object* popInfo);
  static void      initialiseGraphics (Object* gInfo); 
  static Farmland* buildFarm (Object* fInfo);
  static void      buildHex (Object* hInfo);
  static void      buildMilitia (CivilBuilding* target, Object* mInfo);  
  static MilUnit*  buildMilUnit (Object* mInfo);
  static void      buildMilUnitTemplates (Object* info);
  static void      createActionProbabilities (Object* info);
  static void      createPlayer (Object* info);   
  static void      loadAiConstants (Object* info);
  static void      loadTextures (); 
  static void      makeZoneTextures (Object* gInfo); 
  
  static void      writeGameToFile (string fname);
  static void      writeAgeInfoToObject (AgeTracker& age, Object* obj, int skip = 0);  
  static void      writeUnitToObject (MilUnit* unit, Object* obj);

  
private:
  static void addShadows (QGLFramebufferObject* fbo, int texture); 
  static void createCalculator (Object* info, Action::Calculator* ret);
  static double interpolate (double xfrac, double yfrac, int width, int height, double* heightMap); 
  
  static int defaultUnitPriority;
}; 

#endif
