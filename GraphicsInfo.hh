#ifndef GRAPHICS_INFO_HH
#define GRAPHICS_INFO_HH

#include <vector>
#include <QtOpenGL>
#include <QTextStream>
#include "GraphicsBridge.hh"

class MilUnitGraphicsInfo : public GBRIDGE(MilUnit), public TextInfo, public SpriteContainer {
  friend class StaticInitialiser;
public:
  MilUnitGraphicsInfo (MilUnit* dat) : GBRIDGE(MilUnit)(dat), TextInfo() {}
  virtual ~MilUnitGraphicsInfo ();

  virtual void describe (QTextStream& str) const;
  string strengthString (string indent) const;
  void updateSprites (MilStrength* dat);

private:
  static map<MilUnitTemplate*, int> indexMap;
  static vector<vector<doublet> > allFormations;
};


// Following classes don't have a position, so don't descend from GraphicsInfo.

class PlayerGraphicsInfo {
  friend class StaticInitialiser; 
public:
  PlayerGraphicsInfo ();
  ~PlayerGraphicsInfo ();

  int getRed () const {return qRed(colour);}
  int getGreen () const {return qGreen(colour);}
  int getBlue () const {return qBlue(colour);}  
  
private:
  QRgb colour;
  GLuint flag_texture_id; 
};

#endif
