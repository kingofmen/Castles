#include "ThreeDSprite.hh"
#include "boost/tokenizer.hpp"
#include <QtOpenGL>
#include "Logger.hh" 

map<string, GLuint> ThreeDSprite::textureIndices; 
ThreeDSprite::Material* ThreeDSprite::defaultMaterial = new Material(); 

ThreeDSprite::Vertex::Vertex () : x(0), y(0), z(0) {}
ThreeDSprite::Index::Index () : vertex(-1), texture(-1), normal(-1) {}
ThreeDSprite::Group::Group () : special(0), material(defaultMaterial) {}
ThreeDSprite::Material::Material () : colour(1.0, 1.0, 1.0), textureIndex(0) {} 

ThreeDSprite::ThreeDSprite (string fname, vector<string> specials) 
  : numSpecials(0)
  , scaleFactor(1.0, 1.0, 1.0)
{
  loadFile(fname);
  assert(0 < groups.size()); 
  
  int counter = 1;
  for (vector<string>::iterator spec = specials.begin(); spec != specials.end(); ++spec) {
    assert (0 < groups[*spec].faces.size());
    groups[*spec].special = counter++;
  }

  listIndex = glGenLists(1+specials.size());
  assert(0 != listIndex); 
  glNewList(listIndex, GL_COMPILE); 
  for (map<string, Group>::iterator g = groups.begin(); g != groups.end(); ++g) {
    if (0 != (*g).second.special) continue;
    glColor3d((*g).second.material->colour.x(), (*g).second.material->colour.y(), (*g).second.material->colour.z()); 
    glBindTexture(GL_TEXTURE_2D, (*g).second.material->textureIndex);
    for (vector<Face*>::iterator face = (*g).second.faces.begin(); face != (*g).second.faces.end(); ++face) {
      drawFace(*face);
    }
  }
  glEndList();

  for (map<string, Group>::iterator g = groups.begin(); g != groups.end(); ++g) {
    if (0 == (*g).second.special) continue;
    glNewList(listIndex+(*g).second.special, GL_COMPILE);
    glColor3d(1.0, 1.0, 1.0); 
    for (vector<Face*>::iterator face = (*g).second.faces.begin(); face != (*g).second.faces.end(); ++face) {
      drawFace(*face);
    }
    glEndList();
    numSpecials++; 
  }
}

void ThreeDSprite::draw (vector<int>& textures) {
  glScaled(scaleFactor.x(), scaleFactor.y(), scaleFactor.z()); 
  glCallList(listIndex);
  for (int i = 0; i < numSpecials; ++i) {
    if (i >= (int) textures.size()) break; 
    if (-1 == textures[i]) continue;
    glBindTexture(GL_TEXTURE_2D, textures[i]); 
    glCallList(listIndex+i+1); 
  }
}

void ThreeDSprite::drawFace (Face* face) {
  switch (face->verts.size()) {
  case 3 : glBegin(GL_TRIANGLES); break;
  case 4 : glBegin(GL_QUADS);     break;
  default:
    assert(false);
    break;
  }
  for (vector<Index>::iterator ind = face->verts.begin(); ind != face->verts.end(); ++ind) {
    int tex = (*ind).texture;
    if (-1 != tex) glTexCoord2d(textures[tex].x, textures[tex].y);
    tex = (*ind).vertex;
    glVertex3d(vertices[tex].x, vertices[tex].y, vertices[tex].z);
  }
  glEnd(); 
}

void ThreeDSprite::loadMaterials (string fname) {
  ifstream reader;
  reader.open(fname.c_str());  
  string token;
  string matname = "Default";
  
  while (!reader.eof()) {
    reader >> ws; 
    reader >> token;
    if (token.empty()) continue;
    if (token[0] == '#') {
      // Ignore comments
      getline(reader, token);
      continue; 
    }
    if (token == "newmtl") {
      reader >> matname;
      materials[matname] = new Material(); 
      continue;
    }
    // Only really care about diffuse colour
    if (token == "Kd") {
      double red, green, blue;
      reader >> red >> green >> blue;
      materials[matname]->colour.x() = red;
      materials[matname]->colour.y() = green;
      materials[matname]->colour.z() = blue;    
    }
    // Well, and textures.
    else if (token == "map_Kd") {
      reader >> token;
      materials[matname]->textureIndex = getTextureIndex(token); 
    }
    getline(reader, token); // Ignore everything else.     
  }
  reader.close(); 
}

void ThreeDSprite::loadFile (string fname) {
  ifstream reader;
  reader.open(fname.c_str());
  string token;
  string currGroupName("nogroup"); 
  static const string gfx("gfx\\"); 
  
  while (!reader.eof()) {
    reader >> ws; 
    reader >> token;
    if (token.empty()) continue;
    if ((token[0] == '#') || (token == "o")) {
      // Ignore comments
      getline(reader, token);
      continue; 
    }
    if (token == "mtllib") {
      reader >> token; // Filename
      loadMaterials(gfx+token);      
    }
    if (token == "v") {
      Vertex vtx;
      reader >> vtx.x >> vtx.y >> vtx.z;
      getline(reader, token); // Get rid of unused optional w value. 
      vertices.push_back(vtx);
      continue;
    }
    if (token == "vt") {
      Vertex vtx;
      reader >> vtx.x >> vtx.y;
      getline(reader, token); // Optional w value. 
      vtx.z = atof(token.c_str()); 
      textures.push_back(vtx);
      continue;
    }
    if (token == "vn") {
      Vertex vtx;
      reader >> vtx.x >> vtx.y >> vtx.z; 
      normals.push_back(vtx);
      continue;
    }
    if (token == "g") {
      reader >> ws; 
      getline(reader, token);
      currGroupName = token.substr(0, token.find_last_not_of(' ')+1);
      continue; 
    }
    if (token == "usemtl") {
      reader >> token;
      assert(materials[token]); 
      groups[currGroupName].material = materials[token];
      continue; 
    }
    if (token == "f") {
      makeFace(reader, currGroupName); 
      continue; 
    }
  }
  reader.close(); 
}

GLuint ThreeDSprite::getTextureIndex (string fname) {
  if (textureIndices.find(fname) != textureIndices.end()) return textureIndices[fname];

  GLuint newTexture;
  glGenTextures(1, &newTexture);
  textureIndices[fname] = newTexture;
  loadTexture(fname, Qt::red, newTexture); 
  return newTexture; 
}

void ThreeDSprite::makeFace (ifstream& reader, string groupName) {
  reader >> ws;
  string line;
  getline(reader, line); 
  if (line.empty()) return;
 
  boost::char_separator<char> space(" ");
  boost::char_separator<char> slash("/", "", boost::keep_empty_tokens);
  boost::tokenizer<boost::char_separator<char> > vertices(line, space);
  Face* nface = new Face(); 
  for (boost::tokenizer<boost::char_separator<char> >::iterator vtx = vertices.begin(); vtx != vertices.end(); ++vtx) {
    Index curr;
    boost::tokenizer<boost::char_separator<char> > nums(*vtx, slash);
    boost::tokenizer<boost::char_separator<char> >::iterator n = nums.begin(); 
    curr.vertex = atoi((*n).c_str()) - 1; ++n; 
    if (n == nums.end()) goto doneWithIndex;
    if (!(*n).empty()) curr.texture = atoi((*n).c_str()) - 1;
    ++n;
    if (n == nums.end()) goto doneWithIndex;
    curr.normal = atoi((*n).c_str()) - 1;

  doneWithIndex:
    nface->verts.push_back(curr); 
  }

  groups[groupName].faces.push_back(nface); 
}


