#include "ThreeDSprite.hh"
#include "boost/tokenizer.hpp"
#include <QtOpenGL>
#include "Logger.hh" 

ThreeDSprite::Vertex::Vertex () : x(0), y(0), z(0) {}
ThreeDSprite::Index::Index () : vertex(-1), texture(-1), normal(-1) {}
ThreeDSprite::Group::Group () : special(0) {} 

ThreeDSprite::ThreeDSprite (string fname, vector<string> specials) 
  : numSpecials(0)
{
  loadFile(fname);
  assert(0 < groups.size()); 

  //Logger::logStream(Logger::Debug) << "Loaded graphics file " << fname << "\n"; 

  int counter = 1;
  for (vector<string>::iterator spec = specials.begin(); spec != specials.end(); ++spec) {
    /*
    Logger::logStream(Logger::Debug) << "Looking for special '"
				     << (*spec) << "' "
				     << (int) groups[*spec].faces.size() << "\n";
    */
    assert (0 < groups[*spec].faces.size());
    groups[*spec].special = counter++;
  }

  listIndex = glGenLists(1+specials.size());
  assert(0 != listIndex); 
  glNewList(listIndex, GL_COMPILE); 
  for (map<string, Group>::iterator g = groups.begin(); g != groups.end(); ++g) {
    if (0 != (*g).second.special) continue;
    if ((*g).second.colour == "") glColor3d(1.0, 1.0, 1.0);
    else {
      triplet col = colours[(*g).second.colour];
      glColor3d(col.x(), col.y(), col.z()); 
    }
    
    for (vector<Face*>::iterator face = (*g).second.faces.begin(); face != (*g).second.faces.end(); ++face) {
      drawFace(*face);
    }
  }
  glEndList();

  for (map<string, Group>::iterator g = groups.begin(); g != groups.end(); ++g) {
    if (0 == (*g).second.special) continue;
    glNewList(listIndex+(*g).second.special, GL_COMPILE); 
    for (vector<Face*>::iterator face = (*g).second.faces.begin(); face != (*g).second.faces.end(); ++face) {
      drawFace(*face);
    }
    glEndList();
    numSpecials++; 
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
  Logger::logStream(DebugStartup) << "Reading mtllib " << fname << "\n"; 
  
  while (!reader.eof()) {
    reader >> ws; 
    reader >> token;
    Logger::logStream(DebugStartup) << "Found token " << token << "\n"; 
    if (token.empty()) continue;
    if (token[0] == '#') {
      // Ignore comments
      getline(reader, token);
      continue; 
    }
    if (token == "newmtl") {
      reader >> matname;
      continue;
    }
    // Only really care about diffuse colour
    if (token == "Kd") {
      triplet colour;
      double red, green, blue;
      reader >> red >> green >> blue;
      colour.x() = red;
      colour.y() = green;
      colour.z() = blue; 
      Logger::logStream(DebugStartup) << "Read colour " << matname << ": "
				      << colour.x() << " "
				      << colour.y() << " "
				      << colour.z() << "\n";
      colours[matname] = colour; 
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
      groups[currGroupName].colour = token;
      continue; 
    }
    if (token == "f") {
      makeFace(reader, currGroupName); 
      continue; 
    }
  }
  reader.close(); 
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

  //Logger::logStream(Logger::Debug) << "Added face with " << (int) nface->verts.size() << " vertices to '" << groupName << "'\n"; 
  groups[groupName].faces.push_back(nface); 
}


void ThreeDSprite::draw (vector<int>& textures) {
  glCallList(listIndex);
  for (int i = 0; i < numSpecials; ++i) {
    if (i >= (int) textures.size()) break; 
    if (-1 == textures[i]) continue;
    //glBindTexture(GL_TEXTURE_2D, textures[i]); 
    //glCallList(listIndex+i+1); 
  }
}
