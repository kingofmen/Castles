#ifndef GRAPHICSBRIDGE_HH
#define GRAPHICSBRIDGE_HH

#include <type_traits>

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
  GraphicsBridge (Model* m) : gameObject(m), graphicsInfo(0) {}
  GraphicsBridge (Model* m, View* v) : gameObject(m), graphicsInfo(v) {}
  GraphicsBridge () : gameObject(0), graphicsInfo(0) {} // Dummy constructor for mirror objects.
  virtual ~GraphicsBridge () {
    if (graphicsInfo) delete graphicsInfo;
  }

  View* getGraphicsInfo () const {return graphicsInfo;}
  Model* getGameObject () const {return gameObject;}
  bool canReport () const {return (0 != graphicsInfo);}
  void reportEvent (DisplayEvent evt) {
    if (graphicsInfo) graphicsInfo->addEvent(evt);
  }

  void reportEvent (std::string et, std::string de) {
    if (graphicsInfo) reportEvent(DisplayEvent(et, de));
  }

  void initialiseGraphicsBridge () {
    makeGraphicsInfo<Model, View>(gameObject, &graphicsInfo, typename std::is_base_of<GraphicsInfo, View>::type());
    /*
    if () {
      graphicsInfo = new View(gameObject);
    }
    else if (std::is_base_of<TextInfo, View>::value) {
      graphicsInfo = new View();
    }
    */
  }

  void initialiseGraphicsBridge (Model* m) {
    gameObject = m;
    initialiseGraphicsBridge();
  }

  void initialiseTextBridge () {
    graphicsInfo = new View();
  }

protected:

private:
  Model* gameObject;
  View* graphicsInfo;
};

#define GBRIDGE(model) GraphicsBridge<model, model ## GraphicsInfo>
#define TBRIDGE(model) GraphicsBridge<model, TextInfo>

#endif
