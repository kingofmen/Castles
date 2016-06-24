#ifndef UNIT_GRAPHICS_HH
#define UNIT_GRAPHICS_HH

#include <vector>
#include "GraphicsBridge.hh"

class MilUnitGraphicsInfo : public GBRIDGE(MilUnit), public TextInfo, public SpriteContainer {
  friend class StaticInitialiser;
public:
  MilUnitGraphicsInfo (MilUnit* dat);
  virtual ~MilUnitGraphicsInfo ();

  virtual void describe (QTextStream& str) const;
  string strengthString (string indent) const;
  void updateSprites (MilStrength* dat);

private:
  static map<MilUnitTemplate*, int> indexMap;
  static vector<vector<doublet> > allFormations;
};

class TransportUnitGraphicsInfo : public GBRIDGE(TransportUnit) {
  friend class StaticInitialiser;
 public:
  TransportUnitGraphicsInfo(TransportUnit* dat);
  virtual ~TransportUnitGraphicsInfo();

  virtual void describe (QTextStream& str) const {str << "Transport unit";}

 private:
  
};

#endif
