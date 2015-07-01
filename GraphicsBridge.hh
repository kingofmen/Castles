#ifndef GRAPHICSBRIDGE_HH
#define GRAPHICSBRIDGE_HH

struct DisplayEvent {
  DisplayEvent (string et, string de) : eventType(et), details(de) {}
  string eventType;
  string details;
};

template <class Model, class View> class GraphicsBridge {
  // Helper class to hold the boilerplate code for graphics-object access.
public:
  GraphicsBridge (Model* m) : gameObject(m) {graphicsInfo = new View(gameObject);}
  GraphicsBridge () : gameObject(0), graphicsInfo(0) {} // Dummy constructor for mirror objects.
  virtual ~GraphicsBridge () {
    if (graphicsInfo) delete graphicsInfo;
  }

  View* getGraphicsInfo () const {return graphicsInfo;}
  Model* getGameObject () const {return gameObject;}
  bool canReport () const {return (0 != graphicsInfo);}
  void reportEvent (DisplayEvent& evt) {
    if (graphicsInfo) graphicsInfo->addEvent(evt);
  }

private:
  Model* gameObject;
  View* graphicsInfo;
};

#endif
