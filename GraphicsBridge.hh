#ifndef GRAPHICSBRIDGE_HH
#define GRAPHICSBRIDGE_HH

struct DisplayEvent {
  DisplayEvent (std::string et, std::string de) : eventType(et), details(de) {}
  std::string eventType;
  std::string details;
};

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

  void initialiseGraphicsBridge (Model* m) {
    gameObject = m;
    graphicsInfo = new View(gameObject);
  }
  void initialiseGraphicsBridge () {
    graphicsInfo = new View(gameObject);
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