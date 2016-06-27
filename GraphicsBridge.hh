#ifndef GRAPHICSBRIDGE_HH
#define GRAPHICSBRIDGE_HH

#include <type_traits>

#include "graphics/ThreeDSprite.hh"
#include "UtilityFunctions.hh"
#include "Directions.hh"

class FarmGraphicsInfo;
class Hex;
class HexGraphicsInfo;
class Line;
class MilStrength;
class MilUnit;
class MilUnitTemplate;
class TransportUnit;
class Vertex;
class VillageGraphicsInfo;

struct MilUnitSprite {
  ThreeDSprite* soldier;     // Figure for one man - will be drawn several times.
  vector<doublet> positions; // Positions to draw the soldiers, relative to a central point.
};

class SpriteContainer {
public:

  struct spriterator {
    spriterator (int idx, SpriteContainer const* const b) : index(idx), boss(b) {}
    MilUnitSprite* operator* () {if ((int) boss->spriteIndices.size() <= index) return sprites[0]; return sprites[boss->spriteIndices[index]];}
    doublet getFormation () const {return boss->formation[index];}
    void operator++ () {index++;}
    bool operator!= (const spriterator& other) const {return index != other.index;}
    bool operator== (const spriterator& other) const {return index == other.index;}

  private:
    int index;
    SpriteContainer const* const boss;
  };

  spriterator start () const {return spriterator(0, this);}
  spriterator final () const {return spriterator(spriteIndices.size(), this);}

protected:
  vector<int> spriteIndices;
  vector<doublet> formation;
  static vector<MilUnitSprite*> sprites;
};

struct DisplayEvent {
  DisplayEvent (std::string et, std::string de) : eventType(et), details(de) {}
  std::string eventType;
  std::string details;
};

class TextInfo {
public:
  TextInfo () {}
  virtual ~TextInfo ();

  typedef vector<DisplayEvent>::const_iterator EventIter;

  void addEvent (DisplayEvent de);
  virtual void describe (QTextStream& /*str*/) const;
  EventIter startRecentEvents () const {return recentEvents[this].begin();}
  EventIter finalRecentEvents () const {return recentEvents[this].end();}

  static void accumulateEvents (bool acc = true) {accumulate = acc;}
  static void clearRecentEvents ();

protected:
  static map<const TextInfo*, vector<DisplayEvent> > recentEvents;
  static map<const TextInfo*, vector<DisplayEvent> > savingEvents;

private:
  // When true, events are added to the savingEvent container as
  // well as recentEvents. This is intended to apply to events
  // that occur during player actions, so they don't disappear
  // in clearEvents.
  static bool accumulate;
};

class GraphicsInfo {
  friend class StaticInitialiser;
public:
  GraphicsInfo ();
  virtual ~GraphicsInfo ();

  typedef vector<triplet> FieldShape;
  typedef FieldShape::const_iterator cpit;
  typedef FieldShape::iterator pit;

  triplet getNormal () const {return normal;}
  triplet getPosition () const {return position;}
  int getZone () const {return 0;}

  static pair<double, double> getTexCoords (triplet gameCoords, int zone);
  static int getHeight (int x, int y);
  static void getHeightMapCoords (int& hexX, int& hexY, Vertices dir);

  static const int zoneSize = 512; // Size in pixels (for internal purposes, not on the screen).

protected:
  triplet position;
  triplet normal;
  static int zoneSide; // Size in hexes
  static double* heightMap;

  static const double xIncrement;
  static const double yIncrement;
  static const double xSeparation;
  static const double ySeparation;
  static const double zSeparation;
  static const double zOffset;
};

class TextBridge {
public:
  TextBridge () : textInfo(0) {}
  virtual ~TextBridge ();
  void setTextInfo (TextInfo* ti) {textInfo = ti;}
  TextInfo* getTextInfo () const {return textInfo;}
private:
  TextInfo* textInfo;
};

template <class Model, class View> void makeGraphicsInfo (Model* gameObject, View** graphicsInfoPtr, std::true_type const&) {
  (*graphicsInfoPtr) = new View(gameObject);
}

template <class Model, class View> void makeGraphicsInfo (Model*, View** graphicsInfoPtr, std::false_type const&) {
  (*graphicsInfoPtr) = new View();
}

template <class Model, class View> class GraphicsBridge {
  // Helper class to hold the boilerplate code for graphics-object access.
public:
  GraphicsBridge (Model* m) : gameObject(m), viewObject(0) {}
  GraphicsBridge (Model* m, View* v) : gameObject(m), viewObject(v) {}
  GraphicsBridge () : gameObject(0), viewObject(0) {} // Dummy constructor for mirror objects.
  virtual ~GraphicsBridge () {
    if (viewObject) delete viewObject;
  }

  View* getGraphicsInfo () const {return viewObject;}
  Model* getGameObject () const {return gameObject;}
  bool canReport () const {return (0 != viewObject);}
  void reportEvent (DisplayEvent evt) {
    if (viewObject) viewObject->addEvent(evt);
  }

  void reportEvent (std::string et, std::string de) {
    if (viewObject) reportEvent(DisplayEvent(et, de));
  }

  void initialiseBridge () {
    makeGraphicsInfo<Model, View>(gameObject, &viewObject, typename std::is_base_of<GraphicsInfo, View>::type());
  }

  void initialiseBridge (Model* m) {
    gameObject = m;
    viewObject = new View(m);
  }

protected:

private:
  Model* gameObject;
  View* viewObject;
};

double area (GraphicsInfo::FieldShape const& field);

#define GBRIDGE(model) GraphicsBridge<model, model ## GraphicsInfo>
#define TBRIDGE(model) GraphicsBridge<model, TextInfo>

#endif
