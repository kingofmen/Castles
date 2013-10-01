#ifndef THREEDSPRITE_HH
#define THREEDSPRITE_HH

#include <string>
#include <fstream>
#include <vector>
#include <map> 
#include "UtilityFunctions.hh" 
using namespace std; 

class ThreeDSprite {
public:
  ThreeDSprite (string fname, vector<string> specials);
  void draw (vector<int>& textures);
  void setScale (double xsc, double ysc, double zsc) {scaleFactor.x() = xsc; scaleFactor.y() = ysc; scaleFactor.z() = zsc;}
  enum SpecialFlags { First = 1,
		      Second = 2,
		      Third = 4 };
  enum CastleFlags { OneFlag = 1,
		     TwoFlags = 3 }; 
  
private:
  struct Vertex {
    Vertex (); 
    double x, y, z; 
  };
  struct Index {
    Index ();
    int vertex;
    int texture;
    int normal;
  };
  struct Face {
    vector<Index> verts;
  };
  struct Group {
    Group (); 
    vector<Face*> faces;
    int special;
    string colour; 
  };
  
  vector<Vertex> vertices;
  vector<Vertex> textures;
  vector<Vertex> normals; 
  map<string, Group> groups;
  map<string, triplet> colours; 
  int listIndex;
  int numSpecials; 
  
  void loadFile (string fname);
  void loadMaterials (string fname);  
  void makeFace (ifstream& reader, string groupName);
  void drawFace (Face* face);

  triplet scaleFactor;
};

#endif
