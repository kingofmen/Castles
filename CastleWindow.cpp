#include "CastleWindow.hh"
#include "RiderGame.hh"
#include <QPainter>
#include <QVector2D>
#include "glextensions.h"
#include <GL/glu.h> 
#include <cassert> 
#include "MilUnit.hh" 
#include "Hex.hh"
#include "Logger.hh" 
#include "Object.hh"
#include "ThreeDSprite.hh" 
#include "GraphicsInfo.hh" 
#include "StaticInitialiser.hh" 

using namespace std; 
const triplet xaxis(1, 0, 0);
const triplet yaxis(0, 1, 0);
const triplet zaxis(0, 0, -1); // Positive z is into the screen! 
const int zoneSize = 512;
const int buttonSize = 32; 
map<MilUnitTemplate const* const, QIcon> CastleInterface::icons;

WarfareWindow* WarfareWindow::currWindow = 0; 

HexDrawer::HexDrawer (QWidget* p)
  : parent(p)
  , translateX(0)
  , translateY(0)
  , zoomLevel(4)
  , azimuth(0.52) // 30 degrees
  , radial(0)
{}

HexDrawer::~HexDrawer () {}

TextInfoDisplay::TextInfoDisplay (QWidget* p, TextFunc t)
  : QLabel(p)
  , gInfo(0)
  , descFunc(t)
{}

UnitInterface::UnitInterface (QWidget*p)
  : QLabel(p)
  , increasePriorityButton(this)
  , decreasePriorityButton(this)
  , selectedUnit(0)
{
  static QSignalMapper signalMapper; 
  setFixedSize(220, 90);
  increasePriorityButton.move(180, 60);
  increasePriorityButton.resize(buttonSize, buttonSize);
  increasePriorityButton.setIconSize(QSize(buttonSize, buttonSize));  
  increasePriorityButton.setArrowType(Qt::UpArrow);
  signalMapper.setMapping(&increasePriorityButton, 1);
  connect(&increasePriorityButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&increasePriorityButton, SIGNAL(clicked()), p, SLOT(update()));  
  increasePriorityButton.show();
  
  decreasePriorityButton.move(5, 60);
  decreasePriorityButton.resize(buttonSize, buttonSize);
  decreasePriorityButton.setIconSize(QSize(buttonSize, buttonSize));    
  signalMapper.setMapping(&decreasePriorityButton, -1);
  decreasePriorityButton.setArrowType(Qt::DownArrow);
  connect(&decreasePriorityButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&decreasePriorityButton, SIGNAL(clicked()), p, SLOT(update()));    
  decreasePriorityButton.show();

  connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(increasePriority(int))); 
  
  hide(); 
}

EventList::EventList (QWidget* p, int numEvents, int xcoord, int ycoord)
  : selected(0)
{
  for (int i = 0; i < numEvents; ++i) {
    events.push_back(new QLabel(p));
    events.back()->move(xcoord, ycoord + i*15);
    events.back()->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    events.back()->setFixedSize(220, 15);
    events.back()->show();
  }
}

EventList::~EventList () {
  events.clear();
}

void EventList::draw () {
  if (!selected) {
    BOOST_FOREACH(QLabel* ql, events) {
      ql->setText("");
      ql->setToolTip("");
    }
    return;
  }

  GraphicsInfo::EventIter evt = selected->startRecentEvents();
  for (unsigned int label = 0; label < events.size(); ++label) {
    if (evt == selected->finalRecentEvents()) {
      events[label]->setText("");
      events[label]->setToolTip("");
    }
    else {
      events[label]->setText((*evt).eventType.c_str());
      events[label]->setToolTip((*evt).details.c_str());
      ++evt;
    }
  }
}

void EventList::setFont (const QFont& font) {
  for (unsigned int label = 0; label < events.size(); ++label) {
    events[label]->setFont(font);
  }
}

void UnitInterface::increasePriority (int direction) {
  if (!selectedUnit) return;
  selectedUnit->incPriority(direction > 0);
}

void UnitInterface::setUnit (MilUnit* m) {
  selectedUnit = m;
}

CastleInterface::CastleInterface (QWidget*p)
  : QLabel(p)
  , increaseRecruitButton(this)
  , decreaseRecruitButton(this)
  , castle(0)
{
  static QSignalMapper signalMapper; 
  setFixedSize(220, 90);
  increaseRecruitButton.move(180, 60);
  increaseRecruitButton.resize(buttonSize, buttonSize);
  increaseRecruitButton.setIconSize(QSize(buttonSize, buttonSize));    
  increaseRecruitButton.setArrowType(Qt::RightArrow);
  signalMapper.setMapping(&increaseRecruitButton, 1);
  connect(&increaseRecruitButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&increaseRecruitButton, SIGNAL(clicked()), p, SLOT(update())); 
  increaseRecruitButton.show();
  
  decreaseRecruitButton.move(5, 60);
  decreaseRecruitButton.resize(buttonSize, buttonSize);
  decreaseRecruitButton.setIconSize(QSize(buttonSize, buttonSize));    
  decreaseRecruitButton.setArrowType(Qt::LeftArrow);
  signalMapper.setMapping(&decreaseRecruitButton, -1);
  connect(&decreaseRecruitButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&decreaseRecruitButton, SIGNAL(clicked()), p, SLOT(update()));   
  decreaseRecruitButton.show();

  connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(changeRecruitment(int))); 
  
  hide(); 
}

void CastleInterface::changeRecruitment (int direction) {
  if (!castle) return;
  const MilUnitTemplate* curr = castle->getRecruitType();
  MilUnitTemplate::Iterator last = MilUnitTemplate::start();
  for (; last != MilUnitTemplate::final(); ++last) {
    if (curr == (*last)) break;
  }

  if (direction > 0) {
    ++last;
    if (MilUnitTemplate::final() == last) last = MilUnitTemplate::start();
  }
  else {
    if (MilUnitTemplate::start() == last) last = MilUnitTemplate::final();
    --last;
  }
  
  castle->setRecruitType(*last);
  setCastle(castle); // Also sets icons. 
}

void CastleInterface::setCastle (Castle* m) {
  castle = m;

  const MilUnitTemplate* curr = castle->getRecruitType();
  MilUnitTemplate::Iterator last = MilUnitTemplate::start();
  for (; last != MilUnitTemplate::final(); ++last) {
    if (curr == (*last)) break;
  }

  MilUnitTemplate::Iterator next = last; ++next;
  if (next == MilUnitTemplate::final()) next = MilUnitTemplate::start();
  MilUnitTemplate::Iterator prev = last;
  if (last == MilUnitTemplate::start()) prev = MilUnitTemplate::final();
  --prev;

  if (icons.find(*prev) == icons.end()) decreaseRecruitButton.setArrowType(Qt::LeftArrow);
  else {
    decreaseRecruitButton.setArrowType(Qt::NoArrow);
    decreaseRecruitButton.setIcon(icons[*prev]);
  }
  if (icons.find(*next) == icons.end()) increaseRecruitButton.setArrowType(Qt::RightArrow);
  else {
    increaseRecruitButton.setArrowType(Qt::NoArrow);
    increaseRecruitButton.setIcon(icons[*next]);
  }
}

VillageInterface::VillageInterface (QWidget*p)
  : QLabel(p)
  , increaseDrillButton(this)
  , decreaseDrillButton(this)
  , village(0)
{
  static QSignalMapper signalMapper; 
  setFixedSize(220, 90);
  increaseDrillButton.move(180, 60);
  increaseDrillButton.resize(buttonSize, buttonSize);
  increaseDrillButton.setIconSize(QSize(buttonSize, buttonSize));    
  increaseDrillButton.setArrowType(Qt::RightArrow);
  signalMapper.setMapping(&increaseDrillButton, 1);
  connect(&increaseDrillButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&increaseDrillButton, SIGNAL(clicked()), p, SLOT(update())); 
  increaseDrillButton.show();
  
  decreaseDrillButton.move(5, 60);
  decreaseDrillButton.resize(buttonSize, buttonSize);
  decreaseDrillButton.setIconSize(QSize(buttonSize, buttonSize));    
  decreaseDrillButton.setArrowType(Qt::LeftArrow);
  signalMapper.setMapping(&decreaseDrillButton, -1);
  connect(&decreaseDrillButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&decreaseDrillButton, SIGNAL(clicked()), p, SLOT(update()));   
  decreaseDrillButton.show();

  connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(changeDrillLevel(int))); 
  
  hide(); 
}

void VillageInterface::changeDrillLevel (int direction) {
  if (!village) return;
  MilitiaTradition* militia = village->getMilitia();
  if (!militia) return;
  int curr = militia->getDrill(); 

  if (direction > 0) {
    ++curr;
    if (curr <= MilUnitTemplate::getMaxDrill()) militia->increaseDrill(true);
  }
  else {
    curr--;
    if (curr >= 0) militia->increaseDrill(false);
  }
}

void HexDrawer::azimate (double amount) {
  azimuth += amount;
  if (azimuth < 0) azimuth = 0;
  if (azimuth > 1.59) azimuth = 1.59; // 90 degrees.
}

void HexDrawer::zoom (int delta) {
  if (delta > 0) {
    zoomLevel /= 2;
    if (zoomLevel < 1) zoomLevel = 1;
  }
  else {
    zoomLevel *= 2;
    if (zoomLevel > 32) zoomLevel = 32; 
  }
}

void TextInfoDisplay::draw () {
  if (!gInfo) {
    setText("");
    return;
  }
  QString nText;
  QTextStream str(&nText);
  (gInfo->*descFunc)(str);
  setText(nText);
}


GLDrawer::GLDrawer (QWidget* p)
  : HexDrawer(p)
  , QGLWidget(p)
  , cSprite(0)
  , tSprite(0)
  , farmSprite(0)
  , overlayMode(0)
{
  errors = new int[100];
}

void GLDrawer::setTranslate (int x, int y) { 
  translateX -= zoomLevel*(x*cos(radial) + y*sin(radial));
  translateY -= zoomLevel*(y*cos(radial) - x*sin(radial)); 
}

void GLDrawer::drawCastle (Castle* castle) const {
  if (!castle) return;
  CastleGraphicsInfo* cgi = castle->getGraphicsInfo();
  if (!cgi) return;

  triplet castlePos = cgi->getPosition();
  vector<int> texts;
  texts.push_back((*(playerToTextureMap.find(castle->getOwner()))).second);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glTranslated(castlePos.x(), castlePos.y(), castlePos.z());
  
  triplet normal = cgi->getNormal();
  double angle = radToDeg(zaxis.angle(normal));
  triplet axis = zaxis.cross(normal); 
  glRotated(angle, axis.x(), axis.y(), axis.z());

  angle = cgi->getAngle();
  glRotated(angle, 0, 0, 1);
   
  //glBindTexture(GL_TEXTURE_2D, textureIDs[castleTextureIndices[3]]); 
  cSprite->draw(texts);
  glPopMatrix();
  //glDisable(GL_TEXTURE_2D);
  //glBegin(GL_LINES);
  //glVertex3d(dat->getPosition().x(), dat->getPosition().y(), dat->getPosition().z());
  //glVertex3d(dat->getPosition().x()+normal.x(), dat->getPosition().y()+normal.y(), dat->getPosition().z()+normal.z());  
  //glEnd();
  //glEnable(GL_TEXTURE_2D);    

  
}

void GLDrawer::drawLine (LineGraphicsInfo const* dat) {
  glColor4d(1.0, 1.0, 1.0, 1.0);
  glEnable(GL_TEXTURE_2D); 
  
  drawCastle(dat->getLine()->getCastle());
  if (overlayMode) overlayMode->drawLine(dat); 
}

void GLDrawer::drawSprites (const SpriteContainer* info, vector<int>& texts, double angle) {
  for (SpriteContainer::spriterator sprite = info->start(); sprite != info->final(); ++sprite) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glPushMatrix();
    glRotated(angle, 0, 0, 1);
    glTranslated(sprite.getFormation().x(), sprite.getFormation().y(), 0); 
    for (vector<doublet>::iterator p = (*sprite)->positions.begin(); p != (*sprite)->positions.end(); ++p) {
      glPushMatrix();
      glTranslated((*p).x(), (*p).y(), 0); 
      (*sprite)->soldier->draw(texts);
      glPopMatrix();           
    }
    glPopMatrix();
  }
}

void GLDrawer::drawMilUnit (MilUnit* unit, triplet center, double angle) {
  vector<int> texts;  
  texts.push_back(playerToTextureMap[unit->getOwner()]);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glTranslated(center.x(), center.y(), center.z()); 

  glPushMatrix();
  glTranslated(0, 0, -0.7);
  glRotated(-radToDeg(radial), 0, 0, 1);
  glBindTexture(GL_TEXTURE_2D, texts[0]);
  double flagSize = 0.1*sqrt(zoomLevel); 
  glBegin(GL_QUADS);
  glTexCoord2d(0, 0);  
  glVertex3d(0, 0, 0);
  glTexCoord2d(1, 0);  
  glVertex3d(flagSize, 0, 0);
  glTexCoord2d(1, 1);  
  glVertex3d(flagSize, 0, -flagSize);
  glTexCoord2d(0, 1);  
  glVertex3d(0, 0, -flagSize);
  glEnd(); 
  glPopMatrix(); 

  drawSprites(unit->getGraphicsInfo(), texts, angle); 
  glPopMatrix(); 
}
  

void GLDrawer::drawVertex (VertexGraphicsInfo const* gInfo) {
  Vertex* dat = gInfo->getVertex(); 

  MilUnit* unit = dat->getUnit(0);  
  if (!unit) return; 
  glEnable(GL_TEXTURE_2D);   
  triplet center = gInfo->getPosition();

  double angle = 0;
  switch (dat->getUnit(0)->getRear()) {
  case Left:
  case NoVertex:
  default:
    break;
  case Right     : angle = 180; break;
  case RightDown : angle = 240; break;
  case LeftDown  : angle = -60; break;
  case RightUp   : angle = 120; break;
  case LeftUp    : angle =  60; break;
  }

  drawMilUnit(unit, center, angle);   
}

void GLDrawer::drawZone (int which) {
  glColor4d(1.0, 1.0, 1.0, 1.0);
  glEnable(GL_TEXTURE_2D);  
  glBindTexture(GL_TEXTURE_2D, zoneTextures[which]);

  ZoneGraphicsInfo* zoneInfo = ZoneGraphicsInfo::getZoneInfo(which);

  static const double step = 1.0 / zoneSize;
  glBegin(GL_TRIANGLES);
  
  for (int x = 0; x < zoneSize - 1; ++x) {  
    for (int y = 0; y < zoneSize - 1; ++y) {
      glTexCoord2d(x * step, y * step);      
      glVertex3d(zoneInfo->minX + x * step * zoneInfo->width, zoneInfo->minY + y * step * zoneInfo->height, zoneInfo->getHeight(x, y));

      glTexCoord2d(x * step, (y+1) * step); 
      glVertex3d(zoneInfo->minX + x * step * zoneInfo->width, zoneInfo->minY + (y+1) * step * zoneInfo->height, zoneInfo->getHeight(x, (y+1)));
      
      glTexCoord2d((x+1) * step, y * step); 
      glVertex3d(zoneInfo->minX + (x+1) * step * zoneInfo->width, zoneInfo->minY + y * step * zoneInfo->height, zoneInfo->getHeight((x+1), y));

      //glNormal3d(1, 0, 1);
      glTexCoord2d(x * step, (y+1) * step); 
      glVertex3d(zoneInfo->minX + x * step * zoneInfo->width, zoneInfo->minY + (y+1) * step * zoneInfo->height, zoneInfo->getHeight(x, (y+1))); 
      

      glTexCoord2d((x+1) * step, (y+1) * step); 
      glVertex3d(zoneInfo->minX + (x+1) * step * zoneInfo->width, zoneInfo->minY + (y+1) * step * zoneInfo->height, zoneInfo->getHeight((x+1), (y+1)));

      glTexCoord2d((x+1) * step, y * step); 
      glVertex3d(zoneInfo->minX + (x+1) * step * zoneInfo->width, zoneInfo->minY + y * step * zoneInfo->height, zoneInfo->getHeight((x+1), y));
    }
  }

  glEnd();

  glDisable(GL_TEXTURE_2D);
  for (ZoneGraphicsInfo::gridIt grid = zoneInfo->gridBegin(); grid != zoneInfo->gridEnd(); ++grid) {
    glBegin(GL_LINE_LOOP);    
    for (ZoneGraphicsInfo::hexIt hex = (*grid).begin(); hex != (*grid).end(); ++hex) {
      glVertex3d((*hex).x(), (*hex).y(), (*hex).z()); 
    }
    glEnd();    
  }

}

void GLDrawer::drawHex (HexGraphicsInfo const* dat) {

  vector<int> texts; // Not used for trees. 
  glDisable(GL_TEXTURE_2D);    
  glMatrixMode(GL_MODELVIEW);
  for (GraphicsInfo::cpit tree = dat->startTrees(); tree != dat->finalTrees(); ++tree) {
    glPushMatrix();
    glTranslated((*tree).x(), (*tree).y(), (*tree).z());
    tSprite->draw(texts);
    glPopMatrix();
  }
  glColor4d(1.0, 1.0, 1.0, 1.0); 

  
  FarmGraphicsInfo const* farmInfo = dat->getFarmInfo();
  if (!farmInfo) return;
  //Farmland* farm = farmInfo->getFarm();
  VillageGraphicsInfo const* villageInfo = dat->getVillageInfo(); 
  Village* village = villageInfo->getVillage(); 

  glEnable(GL_TEXTURE_2D);
  for (FarmGraphicsInfo::cfit field = farmInfo->start(); field != farmInfo->final(); ++field) {
    glBindTexture(GL_TEXTURE_2D, (*field).getIndex()); 
    glBegin(GL_POLYGON);
    GraphicsInfo::cpit point = (*field).begin(); // Assume square fields, and just unroll loop.
    glTexCoord2d(0, 0);
    glVertex3d((*point).x(), (*point).y(), (*point).z()); ++point;
    glTexCoord2d(0, 1);    
    glVertex3d((*point).x(), (*point).y(), (*point).z()); ++point;
    glTexCoord2d(1, 1);    
    glVertex3d((*point).x(), (*point).y(), (*point).z()); ++point;
    glTexCoord2d(1, 0);    
    glVertex3d((*point).x(), (*point).y(), (*point).z()); 
    glEnd(); 
  }

  glMatrixMode(GL_MODELVIEW);  
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  texts.clear();
  int numHouses = villageInfo->getHouses();
  GraphicsInfo::cpit point1 = villageInfo->startHouse();
  GraphicsInfo::cpit point2 = villageInfo->startHouse(); ++point2;
  GraphicsInfo::cpit point3 = villageInfo->startHouse(); ++point3; ++point3;
  double xstep = ((*point3).x() - (*point2).x()) * 0.1;
  double ystep = ((*point3).y() - (*point2).y()) * 0.1;
  double zstep = ((*point3).z() - (*point2).z()) * 0.1;

  double tranX = ((*point2).x() - (*point1).x())*0.50;
  double tranY = ((*point2).y() - (*point1).y())*0.50;
  double tranZ = ((*point2).z() - (*point1).z())*0.50;

  // Two rows of farmhouses.
  glPushMatrix();  
  glTranslated((*point1).x() + tranX*0.5, (*point1).y() + tranY*0.5, (*point1).z() + tranZ*0.5);
  for (int i = 0; i < numHouses; ++i) {
    glPushMatrix();
    glTranslated(xstep*(i/2) + tranX*(i%2), ystep*(i/2) + tranY*(i%2), zstep*(i/2) + tranZ*(i%2));
    glRotated(180*(i%2), 0, 0, 1);    
    farmSprite->draw(texts); 
    glPopMatrix(); 
  }
  glPopMatrix(); 

  const MilUnitGraphicsInfo* info = village->getMilitiaGraphics();
  glBindTexture(GL_TEXTURE_2D, 0);  
  texts.clear(); 
  texts.push_back(playerToTextureMap[village->getOwner()]);

  point1 = villageInfo->startDrill();
  point2 = villageInfo->startDrill(); ++point2;
  point3 = villageInfo->startDrill(); ++point3; ++point3;

  triplet center = (*point1);
  triplet pointer = (*point3) - (*point2);
  center += pointer*0.6;  
  int sigDeltaY = (fabs(pointer.y()) > 0.00001 ? (pointer.y() > 0 ? 1 : -1) : 0); 
  double angle = 0;
  // I don't fully understand why this code works; the adds are somewhat empirical. 
  if (pointer.x() < 0) angle = -90 - 60*sigDeltaY; 
  else angle = 90 + 60*sigDeltaY;  

  glPushMatrix();
  glTranslated(center.x(), center.y(), center.z());
  drawSprites(info, texts, angle);
  glPopMatrix();
  
  
  glBindTexture(GL_TEXTURE_2D, 0);  
  texts.clear(); 

  point1 = villageInfo->startSheep();
  point2 = villageInfo->startSheep(); ++point2;
  point3 = villageInfo->startSheep(); ++point3; ++point3;

  center = (*point1);
  pointer = (*point2) - (*point3);
  sigDeltaY = (fabs(pointer.y()) > 0.00001 ? (pointer.y() > 0 ? 1 : -1) : 0); 
  angle = 0;
  if (pointer.x() > 0) angle = 90 + 60*sigDeltaY; 
  else angle = -90 - 60*sigDeltaY;  

  glPushMatrix();
  glTranslated(center.x(), center.y(), center.z());
    
  drawSprites(villageInfo, texts, angle);
  glPopMatrix();     
}

void SupplyMode::drawLine (LineGraphicsInfo const* lin) {
  double flowRatio = lin->getFlowRatio();
  if (fabs(flowRatio) < 0.02) return;

  // Draw arrow
  flowRatio *= 0.2; 
  //PlayerGraphicsInfo* pInfo = currentPlayer->getGraphicsInfo();
  
  glDisable(GL_TEXTURE_2D);
  triplet pos = lin->getPosition();
  double angle = lin->getAngle();
  if (flowRatio < 0) angle += 180; 
  //glColor3i(pInfo->getRed(), pInfo->getGreen(), pInfo->getBlue());
  glColor4f(1.0, 1.0, 1.0, 1.0); 
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glTranslated(pos.x(), pos.y(), pos.z());
  glRotated(angle, 0, 0, 1);

  glBegin(GL_POLYGON);
  glVertex3d(0.4, 0, -0.01);
  glVertex3d(0.1, 0.3, -0.01);
  glVertex3d(0.1, flowRatio, -0.01);
  glVertex3d(-0.4, flowRatio, -0.01);
  glVertex3d(-0.4, -flowRatio, -0.01);
  glVertex3d(0.1, -flowRatio, -0.01);
  glVertex3d(0.1, -0.3, -0.01);
  glVertex3d(0.4, 0, -0.01); 
  glEnd();  
  glPopMatrix();
  glEnable(GL_TEXTURE_2D);
  
  glVertex3d(pos.x() + 0.05, pos.y() + 0.05, -0.1);
  glVertex3d(pos.x() + 0.05, pos.y() - 0.05, -0.1);
  glVertex3d(pos.x() - 0.05, pos.y() - 0.05, -0.1);

}

void GLDrawer::convertToOGL (double& x, double& y) {
  static GLint viewport[4];
  static GLdouble modelview[16];
  static GLdouble projection[16];
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
  glGetDoublev(GL_PROJECTION_MATRIX, projection);
  // Convert to OpenGL coordinate system
  y = viewport[3]-y;

  GLdouble oglx = 0;
  GLdouble ogly = 0;
  GLdouble oglz = 0;

  float z = 0; 
  glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &z);
  gluUnProject(x, y, z, modelview, projection, viewport, &oglx, &ogly, &oglz);
  /*Logger::logStream(Logger::Debug) << x << ", " << y << ", " << z << " | "
				   << oglx << ", " << ogly << ", " << oglz << "\n";
  */
  x = oglx;
  y = ogly;
}

Hex* GLDrawer::findHex (double x, double y) {
  if (x < 0) return 0;
  if (y < 0) return 0;
  if (x > width()) return 0;
  if (y > height()) return 0; 
  convertToOGL(x, y); 

  for (HexGraphicsInfo::Iterator hex = HexGraphicsInfo::begin(); hex != HexGraphicsInfo::end(); ++hex) {
    if (!(*hex)->isInside(x, y)) continue;
    return (*hex)->getHex();
  }

  return 0; 
}

Line* GLDrawer::findLine (double x, double y) {
  if (x < 0) return 0;
  if (y < 0) return 0;
  if (x > width()) return 0;
  if (y > height()) return 0; 
  convertToOGL(x, y); 
  
  for (LineGraphicsInfo::Iterator lin = LineGraphicsInfo::begin(); lin != LineGraphicsInfo::end(); ++lin) {
    if (!((*lin)->isInside(x, y))) continue; 
    return (*lin)->getLine(); 
  }
  return 0;
}

Vertex* GLDrawer::findVertex (double x, double y) {
  if (x < 0) return 0;
  if (y < 0) return 0;
  if (x > width()) return 0;
  if (y > height()) return 0; 
  convertToOGL(x, y); 
  
  for (VertexGraphicsInfo::Iterator vex = VertexGraphicsInfo::begin(); vex != VertexGraphicsInfo::end(); ++vex) {
    if (!((*vex)->isInside(x, y))) continue;
    return (*vex)->getVertex(); 
  }
  return 0;
}

int main (int argc, char** argv) { 
  Logger::createStream(Logger::Debug);
  Logger::createStream(Logger::Trace);
  Logger::createStream(Logger::Game);
  Logger::createStream(Logger::Warning);
  Logger::createStream(Logger::Error);
  for (int i = DebugGeneral; i < NumDebugs; ++i) {
    Logger::createStream(i);
    Logger::logStream(i).setActive(false);     
  }
  
  // Write debug log to file
  Logger::createStream(DebugStartup);  
  FileLog debugfile("startDebugLog");
  QObject::connect(&(Logger::logStream(DebugStartup)), SIGNAL(message(QString)), &debugfile, SLOT(message(QString)));

  if (argc > 2) {
    QObject::connect(&(Logger::logStream(Logger::Debug)),   SIGNAL(message(QString)), &debugfile, SLOT(message(QString)));
    QObject::connect(&(Logger::logStream(Logger::Trace)),   SIGNAL(message(QString)), &debugfile, SLOT(message(QString)));
    QObject::connect(&(Logger::logStream(Logger::Game)),    SIGNAL(message(QString)), &debugfile, SLOT(message(QString)));
    QObject::connect(&(Logger::logStream(Logger::Warning)), SIGNAL(message(QString)), &debugfile, SLOT(message(QString)));
    QObject::connect(&(Logger::logStream(Logger::Error)),   SIGNAL(message(QString)), &debugfile, SLOT(message(QString)));
    for (int i = DebugGeneral; i < NumDebugs; ++i) {
      QObject::connect(&(Logger::logStream(i)), SIGNAL(message(QString)), &debugfile, SLOT(message(QString)));
    }
    if (2 == atoi(argv[2])) WarfareGame::unitTests(argv[1]);
    else if (3 == atoi(argv[2])) WarfareGame::functionalTests(argv[1]);
    return 0; 
  }
  
  QApplication industryApp(argc, argv);

  QDesktopWidget* desk = QApplication::desktop();
  QRect scr = desk->availableGeometry();
  WarfareWindow window;
  window.show();

  window.textWindow = new QPlainTextEdit(&window);
  window.textWindow->setFixedSize(900, 140);
  window.textWindow->move(230, 670);
  window.textWindow->show();
  window.textWindow->setFocusPolicy(Qt::NoFocus);
  
  int framewidth = window.frameGeometry().width() - window.geometry().width();
  int frameheight = window.frameGeometry().height() - window.geometry().height();
  window.resize(scr.width() - framewidth, scr.height() - frameheight);
  window.move(0, 0);
  window.setWindowTitle(QApplication::translate("toplevel", "Sinews of War"));

  QMenuBar* menuBar = window.menuBar();
  QMenu* fileMenu = menuBar->addMenu("File");
  QAction* newGame = fileMenu->addAction("New Game");
  QAction* loadGame = fileMenu->addAction("Load Game");
  QAction* saveGame = fileMenu->addAction("Save Game");
  QAction* quit    = fileMenu->addAction("Quit");
  QMenu* actionMenu = menuBar->addMenu("Actions");
  QAction* endTurn = actionMenu->addAction("End turn"); 
  QAction* copyText = actionMenu->addAction("Copy history"); 
  
  QObject::connect(quit, SIGNAL(triggered()), &window, SLOT(close())); 
  QObject::connect(newGame, SIGNAL(triggered()), &window, SLOT(newGame()));
  QObject::connect(loadGame, SIGNAL(triggered()), &window, SLOT(loadGame()));
  QObject::connect(saveGame, SIGNAL(triggered()), &window, SLOT(saveGame()));   
  QObject::connect(endTurn, SIGNAL(triggered()), &window, SLOT(endTurn())); 
  QObject::connect(copyText, SIGNAL(triggered()), &window, SLOT(copyHistory())); 
  
  QPushButton* endTurnButton = new QPushButton("&End turn", &window);
  endTurnButton->setFixedSize(60, 30);
  endTurnButton->move(90, 300);
  QObject::connect(endTurnButton, SIGNAL(clicked()), &window, SLOT(endTurn()));
  QObject::connect(newGame, SIGNAL(triggered()), endTurnButton, SLOT(show()));

  window.show();

  if (argc > 2) window.chooseTask(argv[1], atoi(argv[2])); 
  else if (argc > 1) window.newGame(argv[1]); 

  QObject::connect(&(Logger::logStream(Logger::Debug)),   SIGNAL(message(QString)), &window, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Trace)),   SIGNAL(message(QString)), &window, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Game)),    SIGNAL(message(QString)), &window, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Warning)), SIGNAL(message(QString)), &window, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Error)),   SIGNAL(message(QString)), &window, SLOT(message(QString))); 
  for (int i = DebugGeneral; i < NumDebugs; ++i) {
    QObject::connect(&(Logger::logStream(i)),   SIGNAL(message(QString)), &window, SLOT(message(QString)));
  }
  //Logger::logStream(DebugAI).setActive(true);
  Logger::logStream(DebugTrade).setActive(true); 
  
  return industryApp.exec();  
}


WarfareWindow::WarfareWindow (QWidget* parent)
  : QMainWindow(parent)
  , supplyMode() 
  , selectedHex(0)
  , selectedLine(0)
  , selectedVertex(0)
  , plainMapModeButton(this)
  , supplyMapModeButton(this) 
{
  hexDrawer = new GLDrawer(this);
  hexDrawer->setFixedSize(900, 600);
  hexDrawer->move(230, 30);
  hexDrawer->show();
  hexDrawer->updateGL(); 
  
  selDrawer = new TextInfoDisplay(this, &GraphicsInfo::describe);
  selDrawer->move(15, 30);
  selDrawer->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  selDrawer->show();

  histDrawer = new EventList(this, 7, 1145, 30);
  marketDrawer = new EventList(this, 10, 15, 500);
  QFont fixedWidthFont("Times", 11);
  fixedWidthFont.setFixedPitch(true);
  marketDrawer->setFont(fixedWidthFont);

  unitInterface = new UnitInterface(this);
  unitInterface->move(15, 400);

  castleInterface = new CastleInterface(this);
  castleInterface->move(15, 300); 

  villageInterface = new VillageInterface(this);
  villageInterface->move(15, 300); 
  
  static QSignalMapper signalMapper; 
  supplyMapModeButton.move(272, 635);
  supplyMapModeButton.resize(buttonSize, buttonSize);
  supplyMapModeButton.setIconSize(QSize(buttonSize, buttonSize));    
  supplyMapModeButton.setText("S");
  signalMapper.setMapping(&supplyMapModeButton, 1);
  connect(&supplyMapModeButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  supplyMapModeButton.show();

  plainMapModeButton.move(240, 635);
  plainMapModeButton.resize(buttonSize, buttonSize);
  plainMapModeButton.setIconSize(QSize(buttonSize, buttonSize));    
  plainMapModeButton.setText("P");
  signalMapper.setMapping(&plainMapModeButton, 0);
  connect(&plainMapModeButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  plainMapModeButton.show();
  
  connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(setMapMode(int))); 
  
  currentGame = 0;
  currWindow = this;
  setFocusProxy(0);
  setFocusPolicy(Qt::StrongFocus);
  setFocus(Qt::OtherFocusReason); 
}

WarfareWindow::~WarfareWindow () {}

void WarfareWindow::chooseTask (string fname, int task) {
  switch (task) {
  case 0: newGame(fname); break;
  case 1: WarfareGame::unitComparison(fname); break;
  default: break; 
  }
}

void WarfareWindow::newGame () {
  QString filename = QFileDialog::getOpenFileName(this, tr("Select file"), QString("./scenarios/"), QString("*.txt"));
  string fn(filename.toAscii().data());
  if (fn == "") return;
  newGame(fn); 
}

void WarfareWindow::newGame (string fname) {
  clearGame();
  currentGame = WarfareGame::createGame(fname);
  initialiseGraphics();  
  initialiseColours();
  runNonHumans();
  update();
}

void WarfareWindow::loadGame () {
  QString filename = QFileDialog::getOpenFileName(this, tr("Select file"), QString("./savegames/"), QString("*.txt"));
  string fn(filename.toAscii().data());
  if (fn == "") return;
  newGame(fn);
}

void WarfareWindow::saveGame () {
  QString filename = QFileDialog::getSaveFileName(this, tr("Select file"), QString("./savegames/"), QString("*.txt"));
  string fn(filename.toAscii().data());
  if (fn == "") return;

  StaticInitialiser::writeGameToFile(fn);
}

void WarfareWindow::endTurn () {
  if (!currentGame) return;
  if (!Player::getCurrentPlayer()->isHuman()) return; 
  Action act;
  act.todo = Action::EndTurn;
  act.player = Player::getCurrentPlayer(); 
  humanAction(act); 
  update(); 
}

void WarfareWindow::initialiseGraphics () {
  Object* gInfo = processFile("./common/graphics.txt");
  assert(gInfo); 
  StaticInitialiser::initialiseGraphics(gInfo);
  StaticInitialiser::makeZoneTextures(gInfo);
  StaticInitialiser::makeGraphicsInfoObjects();
  StaticInitialiser::graphicsInitialisation();
  FarmGraphicsInfo::updateFieldStatus();
  VillageGraphicsInfo::updateVillageStatus();
}

void GLDrawer::draw () { 
  paintGL(); 
}

void GLDrawer::setViewport () {
  glEnable(GL_DEPTH_TEST);
  glClearDepth(1000);
  glClearColor(0.0, 0.0, 0.0, 0.0);


  //float lightpos[] = {-1, 0, -0.1, 0 };
  //glLightfv(GL_LIGHT0, GL_POSITION, lightpos); 
  //glEnable(GL_LIGHTING);
  //glEnable(GL_LIGHT0); 
  
  glEnable(GL_TEXTURE_2D); // NB, anything without a texture will be drawn black - temporarily glDisable this for simple debugging shapes 
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-3, 3, -2, 2, 0.1, 10);
  glViewport(0, 0, width(), height());
  glMatrixMode(GL_MODELVIEW); 
  glLoadIdentity();             
  //glTranslatef (0.0, 0.0, -5.0);
  gluLookAt(0.0, 0.0, zoomLevel,
	    0.0, 0.0, 0.0,
	    0.0, -1.0, 0.0); 
}

void GLDrawer::initializeGL () {
  // This is called before any StaticInitialiser code. 
  setViewport(); 
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  
  bool isgood = getGLExtensionFunctions().resolve(this->context()); 
  if (!isgood) Logger::logStream(Logger::Debug) << "Some GL functions not found.\n";
  if (!getGLExtensionFunctions().openGL15Supported()) Logger::logStream(Logger::Debug) << "Could not find OpenGL 1.5 support.\n";
}

void GLDrawer::resizeGL () {
  glViewport(0, 0, width(), height());
}

void GLDrawer::paintGL () {
  if (!cSprite) return; 
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  // I'm sitting about three units from the screen, and the viewport is about 1.5-ish units wide.
  // So the near clipping plane ought to be the same distance in zoomLevel units as its width. But
  // actually this seems to work well, so I won't fiddle with it. 
  glFrustum(-0.125*zoomLevel, 0.125*zoomLevel, -0.0833*zoomLevel, 0.0833*zoomLevel, 0.25*zoomLevel, 2*zoomLevel);
  glMatrixMode(GL_MODELVIEW); 
  glLoadIdentity();

  // We are looking down the z-axis, so positive z is into the screen
  gluLookAt(0.01*translateX+zoomLevel*sin(azimuth)*sin(radial), // Notice x-y switch, to make North point upwards with radial=0
	    0.01*translateY+zoomLevel*sin(azimuth)*cos(radial),
	    -zoomLevel*cos(azimuth), 
	    0.01*translateX, 0.01*translateY, 0.0,
	    -cos(azimuth)*sin(radial), -cos(azimuth)*cos(radial), -sin(azimuth)); 

  glColor4d(0.0, 0.0, 0.0, 0.5);
  glBegin(GL_QUADS);
  
  glVertex3d(-1000, -1000, 0.01);
  glVertex3d( 1000, -1000, 0.01);
  glVertex3d( 1000,  1000, 0.01);
  glVertex3d(-1000,  1000, 0.01);
  glEnd();

  if (LineGraphicsInfo::begin() != LineGraphicsInfo::end()) drawZone(0); 
  
  glColor4d(1.0, 1.0, 1.0, 1.0);
  for (LineGraphicsInfo::Iterator line = LineGraphicsInfo::begin(); line != LineGraphicsInfo::end(); ++line) {
    drawLine(*line); 
  }
  
  for (HexGraphicsInfo::Iterator hex = HexGraphicsInfo::begin(); hex != HexGraphicsInfo::end(); ++hex) {
    drawHex(*hex); 
  }

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  for (VertexGraphicsInfo::Iterator vertex = VertexGraphicsInfo::begin(); vertex != VertexGraphicsInfo::end(); ++vertex) {
    drawVertex(*vertex); 
  }  
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);   
}

void WarfareWindow::paintEvent (QPaintEvent *event) {
  if (!currentGame) return;
  hexDrawer->draw(); 
}

void WarfareWindow::update () {
  hexDrawer->updateGL();
  selDrawer->draw();
  histDrawer->draw();
  marketDrawer->draw();
  QMainWindow::update();
}

void WarfareWindow::wheelEvent (QWheelEvent* event) {
  hexDrawer->zoom(event->delta());
  update(); 
}

void WarfareWindow::selectObject () {
  if (selectedHex) {
    selDrawer->setSelected(selectedHex->getGraphicsInfo());
    villageInterface->setVillage(selectedHex->getVillage());
    villageInterface->show();
    histDrawer->setSelected(selectedHex->getVillage()->getGraphicsInfo());
    marketDrawer->setSelected(selectedHex->getMarket()->getGraphicsInfo());
  }
  else {
    villageInterface->hide();
    marketDrawer->setSelected(0);
  }

  if (selectedVertex) {
    selDrawer->setSelected(selectedVertex->getGraphicsInfo());
    MilUnit* unit = selectedVertex->getUnit(0);    
    if ((unit) && (unit->getOwner() == Player::getCurrentPlayer())) {
      histDrawer->setSelected(unit->getGraphicsInfo());
      unitInterface->setUnit(unit); 
      unitInterface->show();
    }
    else {
      unitInterface->hide();
      histDrawer->setSelected(0);
    }
  }
  else unitInterface->hide();
  
  if (selectedLine) {
    Castle* castle = selectedLine->getCastle();
    if ((castle) && (castle->getOwner() == Player::getCurrentPlayer())) {
      castleInterface->setCastle(castle);
      castleInterface->show();
      histDrawer->setSelected(castle->getGraphicsInfo());
      MilUnit* unit = castle->getGarrison(0);
      if (unit) {
	unitInterface->setUnit(unit);
	unitInterface->show(); 
      }
      else unitInterface->hide(); 
    }
    else castleInterface->hide(); 
    selDrawer->setSelected(selectedLine->getGraphicsInfo());
  }
  else castleInterface->hide(); 
}

void WarfareWindow::mouseReleaseEvent (QMouseEvent* event) {
  int xpos = event->x();
  int ypos = event->y();

  if (!currentGame) return; 

  if (pow(xpos - mouseDownX, 2) + pow(ypos - mouseDownY, 2) > 36) {
    if (Qt::LeftButton & event->button()) {
      hexDrawer->setTranslate(xpos - mouseDownX, ypos - mouseDownY);
      update();
      return;
    }
    else {
      // Building castle. Selected a Vertex,
      // dragging mouse from Hex to Line.
      Hex* clickedHex = hexDrawer->findHex(mouseDownX - hexDrawer->x(), mouseDownY - hexDrawer->y());
      if (!clickedHex) return; 
      Line* clickedLine = hexDrawer->findLine(xpos - hexDrawer->x(), ypos - hexDrawer->y());
      if (!clickedLine) return; 
      if ((!selectedVertex) || (0 == selectedVertex->numUnits())) { 
	Logger::logStream(Logger::Warning) << "Must select unit to build castle.\n";
	return;
      }
      if (NoDirection == clickedHex->getDirection(clickedLine)) {
	Logger::logStream(Logger::Warning) << "That Hex cannot support that Line.\n"; 
	return;
      }
      if (clickedLine->getCastle()) {
	Logger::logStream(Logger::Warning) << "That Line already has a castle.\n";
	return;
      }
      for (Hex::LineIterator lin = clickedHex->linBegin(); lin != clickedHex->linEnd(); ++lin) {
	if (!(*lin)->getCastle()) continue;
	if (clickedHex != (*lin)->getCastle()->getSupport()) continue;
	Logger::logStream(Logger::Warning) << "That Hex already supports a castle.\n"; 
	return;
      }
      //if (8 < clickedHex->getDevastation()) {
      //Logger::logStream(Logger::Warning) << "Hex is too devastated.\n"; 
      //return;
      //}
      Action act;
      act.player = Player::getCurrentPlayer();
      act.todo = Action::BuildFortress;
      act.source = clickedHex;
      act.cease = clickedLine;
      act.start = selectedVertex;
      humanAction(act);    
      update(); 
      return; 
    }
  }

  Hex* clickedHex = hexDrawer->findHex(xpos - hexDrawer->x(), ypos - hexDrawer->y());
  Vertex* clickedVertex = hexDrawer->findVertex(xpos - hexDrawer->x(), ypos - hexDrawer->y());
  Line* clickedLine = hexDrawer->findLine(xpos - hexDrawer->x(), ypos - hexDrawer->y());

  if (Qt::LeftButton & event->button()) {
    selectedHex = clickedHex;
    selectedLine = clickedLine;
    selectedVertex = clickedVertex;
    selectObject(); 
    update();
    return; 
  }

  Action act;
  act.player = Player::getCurrentPlayer();
  
  if (clickedHex) {
    if (selectedVertex) {
      // Vertex to Hex: Devastate
      if (NoVertex == clickedHex->getDirection(selectedVertex)) {
	Logger::logStream(Logger::Warning) << "Not adjacent, cannot devastate.\n";
	return;
      }
      if (0 == selectedVertex->numUnits()) {
	Logger::logStream(Logger::Warning) << "No unit selected.\n";
	return;
      }
      if (Player::getCurrentPlayer() != selectedVertex->getUnit(0)->getOwner()) {
	Logger::logStream(Logger::Warning) << "Not your unit.\n";
	return; 
      }
      act.todo = Action::Devastate;
      act.start = selectedVertex;
      act.target = clickedHex; 
    }
    else if ((selectedHex) && (selectedHex == clickedHex)) {
      //if (0 >= clickedHex->getDevastation()) {
      //Logger::logStream(Logger::Warning) << "Hex is not damaged, cannot be repaired.\n"; 
      //return;
      //}
      //act.todo = Action::Repair;
      //act.target = clickedHex;
      //act.source = clickedHex;
      return; 
    }
  }
  else if (clickedVertex) {
    if (selectedLine) {
      // Line to Vertex - mobilise
      if (!selectedLine->getCastle()) {
	Logger::logStream(Logger::Warning) << "Cannot mobilise without castle.\n";
	return;
      }
      if (selectedLine->getCastle()->getOwner() != Player::getCurrentPlayer()) {
	Logger::logStream(Logger::Warning) << "Not your castle, cannot mobilise.\n"; 
	return;
      }
      if (0 == selectedLine->getCastle()->numGarrison()) {
	Logger::logStream(Logger::Warning) << "Empty castle, cannot be mobilised.\n"; 
	return;
      }
      if ((0 < clickedVertex->numUnits()) && (clickedVertex->getUnit(0)->getOwner() == Player::getCurrentPlayer())) {
      	Logger::logStream(Logger::Warning) << "Friendly unit is in the way.\n"; 
	return;
      }

      act.todo = Action::Mobilise;
      act.begin = selectedLine;
      act.final = clickedVertex; 
    }
    else if ((selectedVertex) && (selectedVertex != clickedVertex)) {
      // Vertex to Vertex - move
      if (0 == selectedVertex->numUnits()) {
	Logger::logStream(Logger::Warning) << "No unit, cannot move.\n";
	return; 
      }
      if (Player::getCurrentPlayer() != selectedVertex->getUnit(0)->getOwner()) {
	Logger::logStream(Logger::Warning) << "Not your unit.\n";
	return; 
      }
      if (NoVertex == selectedVertex->getDirection(clickedVertex)) {
	Logger::logStream(Logger::Warning) << "Not adjacent.\n";
	return; 
      }
      if ((0 < clickedVertex->numUnits()) && (clickedVertex->getUnit(0)->getOwner() == Player::getCurrentPlayer())) {
      	Logger::logStream(Logger::Warning) << "Friendly unit is in the way.\n"; 
	return;
      }
      Line* middle = selectedVertex->getLine(clickedVertex);
      if (!middle) {
	Logger::logStream(Logger::Warning) << "Not adjacent.\n";
	return; 
      }
      if ((middle->getCastle()) && (middle->getCastle()->getOwner() != Player::getCurrentPlayer())) {
	Logger::logStream(Logger::Warning) << "Enemy castle in the way.\n"; 
	return; 
      }
      
      act.todo = Action::Attack;
      act.start = selectedVertex;
      act.final = clickedVertex; 
    }
    else if ((selectedVertex) && (selectedVertex == clickedVertex)) {
      // One Vertex - reinforce
      if (0 == selectedVertex->numUnits()) {
	Logger::logStream(Logger::Warning) << "No unit, cannot reinforce.\n";
	return; 
      }
      if (Player::getCurrentPlayer() != selectedVertex->getUnit(0)->getOwner()) {
	Logger::logStream(Logger::Warning) << "Not your unit.\n";
	return; 
      }

      act.todo = Action::Reinforce;
      act.start = selectedVertex;
      act.final = clickedVertex; 
    }
  }
  else if (clickedLine) {
    if (selectedVertex) {
      // Vertex to Line, call for surrender, or possibly enter garrison. 
      if ((selectedVertex != clickedLine->oneEnd()) && (selectedVertex != clickedLine->twoEnd())) {
	Logger::logStream(Logger::Warning) << "Not adjacent.\n"; 
	return; 
      }
      if (!clickedLine->getCastle()) {
	Logger::logStream(Logger::Warning) << "No castle there, cannot call for surrender.\n";
	return; 
      }
      if (0 == selectedVertex->numUnits()) {
	Logger::logStream(Logger::Warning) << "Must have unit to call for surrender.\n"; 
	return; 
      }
      if (Player::getCurrentPlayer() != selectedVertex->getUnit(0)->getOwner()) {
	Logger::logStream(Logger::Warning) << "Not your unit.\n"; 
	return; 
      }
      act.cease = clickedLine;
      act.start = selectedVertex;
      if (clickedLine->getCastle()->getOwner() == Player::getCurrentPlayer()) {
	// This is a move into garrison, not call for surrender. 
	act.todo = Action::EnterGarrison; 
      }
      else {
	act.todo = Action::CallForSurrender;
      }
    }
    else if (selectedLine == clickedLine) {
      if (!clickedLine->getCastle()) {
	Logger::logStream(Logger::Warning) << "No castle there, cannot reinforce.\n";
	return; 
      }
      if (clickedLine->getCastle()->getOwner() != Player::getCurrentPlayer()) {
	Logger::logStream(Logger::Warning) << "Not your castle, cannot reinforce.\n";
	return; 
      }
      if ((0 < clickedLine->oneEnd()->numUnits()) && (Player::getCurrentPlayer() != clickedLine->oneEnd()->getUnit(0)->getOwner()) &&
	  (0 < clickedLine->twoEnd()->numUnits()) && (Player::getCurrentPlayer() != clickedLine->twoEnd()->getUnit(0)->getOwner())) {
	Logger::logStream(Logger::Warning) << "Castle is besieged, cannot reinforce.\n"; 
	return; 
      }

      act.todo = Action::Recruit;
      act.begin = clickedLine;
      act.cease = clickedLine; 
    }
  }
  else return; 
  
  humanAction(act);    
  update(); 
  return; 

}

void WarfareWindow::mousePressEvent (QMouseEvent* event) {
  mouseDownX = event->x();
  mouseDownY = event->y();
}

void WarfareWindow::keyReleaseEvent (QKeyEvent* event) {
  if (!currentGame) return; 
  
  switch (event->key()) {
  case Qt::Key_Up:
    hexDrawer->azimate(-0.01);
    break;
    
  case Qt::Key_Down:
    hexDrawer->azimate(0.01);
    break;

  case Qt::Key_Left:
    hexDrawer->rotate(-0.02);
    break;

  case Qt::Key_Right:
    hexDrawer->rotate(0.02);
    break; 

  case Qt::Key_Plus:
    hexDrawer->zoom(1);
    break;

  case Qt::Key_Minus:
    hexDrawer->zoom(-1);
    break;
    
  default:
    QWidget::keyReleaseEvent(event);
    return; 
  }

  update();
}

bool WarfareWindow::turnEnded () {
  if (!currentGame) return false;

  for (Player::Iter pl = Player::start(); pl != Player::final(); ++pl) {
    if ((*pl)->turnEnded()) continue;
    return false; 
  }
  
  return true; 
}

void WarfareWindow::endOfTurn () {
  try {
    currentGame->endOfTurn();
  }
  catch (string errorMessage) {
    Logger::logStream(Logger::Error) << "Exception with message " << errorMessage << ". Game is probably in a bad state.\n";
  }    
  for (Player::Iter pl = Player::start(); pl != Player::final(); ++pl) {
    (*pl)->newTurn(); 
  }
}

Player* WarfareWindow::gameOver () {
  set<Player*> stillHaveCastles;
  for (Line::Iterator lin = Line::start(); lin != Line::final(); ++lin) {
    Castle* curr = (*lin)->getCastle();
    if (!curr) continue;
    stillHaveCastles.insert(curr->getOwner());
  }
  assert(0 < stillHaveCastles.size()); 
  if (1 != stillHaveCastles.size()) return 0;
  return (*(stillHaveCastles.begin())); 
}

void WarfareWindow::humanAction (Action& act) {
  if (!currentGame) return;
  Action::ActionResult res = act.execute();
  if ((Action::Ok != res) && (Action::AttackFails != res)) {
    Logger::logStream(Logger::Game) << "Action failed " << res << "\n";
    return; 
  }
  act.player->finished();
  Player* winner = gameOver(); 
  if (winner) {
    Logger::logStream(Logger::Game) << winner->getDisplayName() << " wins.\n";
    delete currentGame;
    currentGame = 0;
    hexDrawer->draw(); 
    return; 
  }
  if (turnEnded()) endOfTurn(); 

  Player::advancePlayer();
  runNonHumans();
  selectObject();
}

void WarfareWindow::runNonHumans () {
  while (!Player::getCurrentPlayer()->isHuman()) {
    Logger::logStream(Logger::Debug) << Player::getCurrentPlayer()->getDisplayName() << "\n";
    Player::getCurrentPlayer()->getAction();
    if (turnEnded()) endOfTurn();
    Player::advancePlayer();
    Logger::logStream(Logger::Debug) << Player::getCurrentPlayer()->getDisplayName() << "\n";
  }
}

void WarfareWindow::message (QString m) {
  textWindow->appendPlainText(m); 
}

void WarfareWindow::initialiseColours () {
  hexDrawer->setFixedSize(899, 600);
  hexDrawer->setFixedSize(900, 600);
  selDrawer->setFixedSize(220, 300);
}

void WarfareWindow::clearGame () {
    if (currentGame) {
    delete currentGame;
    currentGame = 0;
    hexDrawer->draw(); 
  }
}

void WarfareWindow::setMapMode (int m) {
  switch (m) {
  case 1:
    hexDrawer->setOverlayMode(&supplyMode);
    break; 
  case 0:
  default:
    hexDrawer->setOverlayMode(0);
    break;
  }
  hexDrawer->updateGL(); 
}

void WarfareWindow::copyHistory () {
  textWindow->selectAll();
  textWindow->copy(); 
}
