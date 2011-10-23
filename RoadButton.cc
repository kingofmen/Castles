#include "RoadButton.hh"


RoadButton::RoadButton (Hex::Vertices s, Hex::Vertices f, QWidget* parent)
  : QPushButton(parent)
  , start(s)
  , final(f)
{
  connect(this, SIGNAL(clicked()), this, SLOT(reEmitClicked())); 
}

RoadButton::RoadButton (Hex::Vertices s, QWidget* parent)
  : QPushButton(parent)
  , start(s)
  , final(Hex::NoVertex)
{
  connect(this, SIGNAL(clicked()), this, SLOT(reEmitClicked())); 
}


void RoadButton::reEmitClicked () {
  if (Hex::NoVertex == final) emit buildCastle(start); 
  else emit buildRoad(start, final);
}
