#include "RoadButton.hh"


RoadButton::RoadButton (Vertices s, Vertices f, QWidget* parent)
  : QPushButton(parent)
  , start(s)
  , final(f)
{
  connect(this, SIGNAL(clicked()), this, SLOT(reEmitClicked())); 
}

RoadButton::RoadButton (Vertices s, QWidget* parent)
  : QPushButton(parent)
  , start(s)
  , final(NoVertex)
{
  connect(this, SIGNAL(clicked()), this, SLOT(reEmitClicked())); 
}


void RoadButton::reEmitClicked () {
  if (NoVertex == final) emit buildCastle(start); 
  else emit buildRoad(start, final);
}
