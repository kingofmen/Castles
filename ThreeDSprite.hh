#ifndef THREEDSPRITE_HH
#define THREEDSPRITE_HH

#include <string>
#include <fstream>
#include <vector>
#include <map> 

class ThreeDSprite {
public:
  ThreeDSprite (std::string fname, std::vector<std::string> specials);
  void draw (std::vector<int>& textures); 
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
    std::vector<Index> verts;
  };
  struct Group {
    Group (); 
    std::vector<Face*> faces;
    int special; 
  };
  
  
  std::vector<Vertex> vertices;
  std::vector<Vertex> textures;
  std::vector<Vertex> normals; 
  std::map<std::string, Group> groups;
  int listIndex;
  int numSpecials; 
  
  void loadFile (std::string fname);
  void makeFace (std::ifstream& reader, std::string groupName);
  void drawFace (Face* face); 
};

#endif
