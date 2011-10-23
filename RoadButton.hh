#ifndef ROAD_BUTTON_HH
#define ROAD_BUTTON_HH

#include "Hex.hh"
#include "QPushButton.h"

class RoadButton : public QPushButton {
  Q_OBJECT

public:
  RoadButton (Hex::Vertices s, Hex::Vertices f, QWidget* parent); 
  RoadButton (Hex::Vertices s, QWidget* parent); 
  
signals:
  void buildRoad (Hex::Vertices start, Hex::Vertices final); 
  void buildCastle (Hex::Vertices dere); 
  
private slots:
  void reEmitClicked (); 
  
private:
  Hex::Vertices start;
  Hex::Vertices final;
};

#endif
