#ifndef THREEDSPRITE_HH
#define THREEDSPRITE_HH

#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <QtOpenGL>
#include "UtilityFunctions.hh"
using namespace std; 

class ThreeDSprite {
public:
  ThreeDSprite (string fname, vector<string> specials);
  void draw (vector<int>& textures);
  void setScale (double xsc, double ysc, double zsc) {scaleFactor.x() = xsc; scaleFactor.y() = ysc; scaleFactor.z() = zsc;}
  
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
  struct Material {
    Material ();
    triplet colour;
    GLuint textureIndex;
  };
  struct Group {
    Group (); 
    vector<Face*> faces;
    int special;
    Material* material; 
  };
  
  vector<Vertex> vertices;
  vector<Vertex> textures;
  vector<Vertex> normals; 
  map<string, Group> groups;
  map<string, Material*> materials; 
  int listIndex;
  int numSpecials; 
  
  void loadFile (string fname);
  void loadMaterials (string fname);  
  void makeFace (ifstream& reader, string groupName);
  void drawFace (Face* face);

  triplet scaleFactor;

  static GLuint getTextureIndex (string fname);
  static map<string, GLuint> textureIndices;
  static Material* defaultMaterial; 
};

#endif
