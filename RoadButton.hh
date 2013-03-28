#ifndef ROAD_BUTTON_HH
#define ROAD_BUTTON_HH

#include "Hex.hh"
#include "QPushButton.h"

class RoadButton : public QPushButton {
  Q_OBJECT

public:
  RoadButton (Vertices s, Vertices f, QWidget* parent); 
  RoadButton (Vertices s, QWidget* parent); 
  
signals:
  void buildRoad (Vertices start, Vertices final); 
  void buildCastle (Vertices dere); 
  
private slots:
  void reEmitClicked (); 
  
private:
  Vertices start;
  Vertices final;
};

#endif
