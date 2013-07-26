#include "CastleWindow.hh"
#include "RiderGame.hh" 
#include <QPainter>
#include <QVector2D> 
#include "glextensions.h"
#include <GL/glu.h> 
#include <cassert> 
#include "PopUnit.hh"
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

WarfareWindow* WarfareWindow::currWindow = 0; 

HexDrawer::HexDrawer (QWidget* p)
  : parent(p)
  , translateX(0)
  , translateY(0)
  , zoomLevel(4)
  , azimuth(0)
  , radial(0)
{}

HexDrawer::~HexDrawer () {}

SelectedDrawer::SelectedDrawer (QWidget* p)
  : QLabel(p)
  , gInfo(0) 
{}

UnitInterface::UnitInterface (QWidget*p)
  : QLabel(p)
  , increasePriorityButton(this)
  , decreasePriorityButton(this)
  , unit(0)
{
  static QSignalMapper signalMapper; 
  setFixedSize(220, 90);
  increasePriorityButton.move(180, 60);
  increasePriorityButton.resize(20, 20);
  increasePriorityButton.setArrowType(Qt::UpArrow);
  signalMapper.setMapping(&increasePriorityButton, 1);
  connect(&increasePriorityButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&increasePriorityButton, SIGNAL(clicked()), p, SLOT(update()));  
  increasePriorityButton.show();
  
  decreasePriorityButton.move(5, 60);
  decreasePriorityButton.resize(20, 20);
  decreasePriorityButton.setArrowType(Qt::DownArrow);
  signalMapper.setMapping(&decreasePriorityButton, -1);
  connect(&decreasePriorityButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&decreasePriorityButton, SIGNAL(clicked()), p, SLOT(update()));    
  decreasePriorityButton.show();

  connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(increasePriority(int))); 
  
  hide(); 
}

void UnitInterface::increasePriority (int direction) {
  if (!unit) return;
  unit->incPriority(direction > 0);
}

void UnitInterface::setUnit (MilUnit* m) {
  unit = m;
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
  increaseRecruitButton.resize(20, 20);
  increaseRecruitButton.setArrowType(Qt::RightArrow);
  signalMapper.setMapping(&increaseRecruitButton, 1);
  connect(&increaseRecruitButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&increaseRecruitButton, SIGNAL(clicked()), p, SLOT(update())); 
  increaseRecruitButton.show();
  
  decreaseRecruitButton.move(5, 60);
  decreaseRecruitButton.resize(20, 20);
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
  MilUnitTemplate::Iterator final = MilUnitTemplate::begin();
  for (; final != MilUnitTemplate::end(); ++final) {
    if (curr == (*final)) break; 
  }

  if (direction > 0) {
    ++final;
    if (MilUnitTemplate::end() == final) final = MilUnitTemplate::begin();
  }
  else {
    if (MilUnitTemplate::begin() == final) final = MilUnitTemplate::end();
    --final; 
  }
  castle->setRecruitType(*final);   
}

void CastleInterface::setCastle (Castle* m) {
  castle = m;
}

FarmInterface::FarmInterface (QWidget*p)
  : QLabel(p)
  , increaseDrillButton(this)
  , decreaseDrillButton(this)
  , farm(0)
{
  static QSignalMapper signalMapper; 
  setFixedSize(220, 90);
  increaseDrillButton.move(180, 60);
  increaseDrillButton.resize(20, 20);
  increaseDrillButton.setArrowType(Qt::RightArrow);
  signalMapper.setMapping(&increaseDrillButton, 1);
  connect(&increaseDrillButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&increaseDrillButton, SIGNAL(clicked()), p, SLOT(update())); 
  increaseDrillButton.show();
  
  decreaseDrillButton.move(5, 60);
  decreaseDrillButton.resize(20, 20);
  decreaseDrillButton.setArrowType(Qt::LeftArrow);
  signalMapper.setMapping(&decreaseDrillButton, -1);
  connect(&decreaseDrillButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  connect(&decreaseDrillButton, SIGNAL(clicked()), p, SLOT(update()));   
  decreaseDrillButton.show();

  connect(&signalMapper, SIGNAL(mapped(int)), this, SLOT(changeDrillLevel(int))); 
  
  hide(); 
}

void FarmInterface::changeDrillLevel (int direction) {
  if (!farm) return;
  MilitiaTradition* militia = farm->getMilitia();
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

void FarmInterface::setFarm (Farmland* m) {
  farm = m;
}


void HexDrawer::azimate (double amount) {
  azimuth += amount;
  if (azimuth < 0) azimuth = 0;
  if (azimuth > 1.0471) azimuth = 1.0471; // 60 degrees. 
}

void HexDrawer::zoom (int delta) {
  if (delta > 0) {
    zoomLevel /= 2;
    if (zoomLevel < 1) zoomLevel = 1;
  }
  else {
    zoomLevel *= 2;
    if (zoomLevel > 64) zoomLevel = 64; 
  }
}

void SelectedDrawer::draw () {
  //Logger::logStream(Logger::Debug) << "Got here\n"; 
  if (!gInfo) {
    setText("");
    return;
  }
  QString nText;
  QTextStream str(&nText);
  gInfo->describe(str); 
  setText(nText); 
}


GLDrawer::GLDrawer (QWidget* p)
  : HexDrawer(p)
  , QGLWidget(p)
  , assignedTextures(0)
  , numTextures(100)
  , cSprite(0)
  , kSprite(0)
  , overlayMode(0)
{
  textureIDs = new GLuint[numTextures];
  errors = new int[100];
}

void GLDrawer::setTranslate (int x, int y) { 
  translateX -= zoomLevel*(x*cos(radial) + y*sin(radial));
  translateY -= zoomLevel*(y*cos(radial) - x*sin(radial)); 
}

void GLDrawer::drawCastle (Castle* castle, LineGraphicsInfo const* dat) {
  if (!castle) return;

  double castleX = 0;
  double castleY = 0;
  double castleZ = 0;   
  dat->getCastlePosition(castleX, castleY, castleZ);

  vector<int> texts;
  for (int i = 0; i < castle->numGarrison(); ++i) {
    texts.push_back(textureIDs[playerToTextureMap[castle->getOwner()]]); 
  }

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glTranslated(castleX, castleY, castleZ);
  
  triplet normal = dat->getNormal();
  double angle = radToDeg(zaxis.angle(normal));
  triplet axis = zaxis.cross(normal); 
  glRotated(angle, axis.x(), axis.y(), axis.z());

  angle = dat->getAngle();  
  glRotated(angle, 0, 0, 1);
   
  glBindTexture(GL_TEXTURE_2D, textureIDs[castleTextureIndices[3]]); 
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
  
  drawCastle(dat->getLine()->getCastle(), dat);
  if (overlayMode) overlayMode->drawLine(dat); 
}

void GLDrawer::drawVertex (VertexGraphicsInfo const* gInfo) {
  Vertex* dat = gInfo->getVertex(); 
  
  if (0 == dat->numUnits()) return;
  glEnable(GL_TEXTURE_2D);   
  triplet center = gInfo->getPosition();

  vector<int> texts;
  texts.push_back(textureIDs[playerToTextureMap[dat->getUnit(0)->getOwner()]]);
  texts.push_back(textureIDs[playerToTextureMap[dat->getUnit(0)->getOwner()]]); 


  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glTranslated(center.x(), center.y(), center.z()); 
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
  
  glBindTexture(GL_TEXTURE_2D, 0);
  glTranslated(0, 0.1, 0);
  glRotated(angle, 0, 0, 1); 
  kSprite->draw(texts);
  glRotated(-angle, 0, 0, 1); 
  glTranslated(0, -0.2, 0);
  glRotated(angle, 0, 0, 1); 
  glBindTexture(GL_TEXTURE_2D, 0);
  kSprite->draw(texts);
  glPopMatrix(); 
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
      //triplet vec1 = point3;
      //vec1 -= point2;
      //triplet vec2 = point1;
      //vec2 -= point2;
      //vec1 = vec2.cross(vec1);
      //vec1.normalise(); 
      //glNormal3d(vec1.x(), vec1.y(), vec1.z());
      
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
  Farmland* farm = dat->getHex()->getFarm();
  if (!farm) return; 
}

ThreeDSprite* GLDrawer::makeSprite (Object* info) {
  string castleFile = info->safeGetString("filename", "nosuchbeast");
  assert(castleFile != "nosuchbeast");
  vector<string> specs;
  objvec svec = info->getValue("separate");
  for (objiter s = svec.begin(); s != svec.end(); ++s) {
    specs.push_back((*s)->getLeaf()); 
  }
  ThreeDSprite* ret = new ThreeDSprite(castleFile, specs);
  return ret; 
}

void GLDrawer::loadSprites () {
  if ((cSprite) && (kSprite)) return;
  if (cSprite) delete cSprite;
  if (kSprite) delete kSprite;
  
  Object* ginfo = processFile("gfx/info.txt");
  Object* castleInfo = ginfo->safeGetObject("castlesprite");
  assert(castleInfo);
  cSprite = makeSprite(castleInfo); 
  Object* knightinfo = ginfo->safeGetObject("knightsprite");
  assert(knightinfo);
  kSprite = makeSprite(knightinfo); 
}

void GLDrawer::assignColour (Player* p) {
  string pName = "gfx/" + p->getName() + ".png";
  GLuint texid; 
  glGenTextures(1, &texid);
  texid = loadTexture(pName, Qt::red); 
  playerToTextureMap[p] = texid; 
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

  // Write debug log to file
  Logger::createStream(DebugStartup);  
  FileLog debugfile("startDebugLog");
  QObject::connect(&(Logger::logStream(DebugStartup)), SIGNAL(message(QString)), &debugfile, SLOT(message(QString)));

  
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

  for (int i = DebugLeaders; i < NumDebugs; ++i) {
    Logger::createStream(i);
    QObject::connect(&(Logger::logStream(i)),   SIGNAL(message(QString)), &window, SLOT(message(QString)));
    Logger::logStream(i).setActive(false); 
  }
  Logger::logStream(DebugAI).setActive(true); 
  
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
  
  selDrawer = new SelectedDrawer(this);
  selDrawer->move(15, 30);
  selDrawer->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  selDrawer->show();

  unitInterface = new UnitInterface(this);
  unitInterface->move(15, 400);

  castleInterface = new CastleInterface(this);
  castleInterface->move(15, 300); 

  farmInterface = new FarmInterface(this);
  farmInterface->move(15, 300); 
  
  static QSignalMapper signalMapper; 
  supplyMapModeButton.move(270, 640);
  supplyMapModeButton.resize(20, 20);
  supplyMapModeButton.setText("S");
  signalMapper.setMapping(&supplyMapModeButton, 1);
  connect(&supplyMapModeButton, SIGNAL(clicked()), &signalMapper, SLOT(map()));
  supplyMapModeButton.show();

  plainMapModeButton.move(240, 640);
  plainMapModeButton.resize(20, 20);
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
  currentGame = WarfareGame::createGame(fname, currentPlayer);
  initialiseColours();
  runNonHumans();
  update();
}

void WarfareWindow::loadGame () {
  QString filename = QFileDialog::getOpenFileName(this, tr("Select file"), QString("./savegames/"), QString("*.txt"));
  string fn(filename.toAscii().data());
  if (fn == "") return;

  clearGame();
  currentGame = WarfareGame::createGame(fn, currentPlayer);
  initialiseColours();
  runNonHumans();
  update();
}

void WarfareWindow::saveGame () {
  QString filename = QFileDialog::getSaveFileName(this, tr("Select file"), QString("./savegames/"), QString("*.txt"));
  string fn(filename.toAscii().data());
  if (fn == "") return;

  StaticInitialiser::writeGameToFile(fn);
  //WarfareGame::saveGame(fn, currentPlayer);
}

void WarfareWindow::endTurn () {
  if (!currentGame) return;
  if (!currentPlayer->isHuman()) return; 
  Action act;
  act.todo = Action::EndTurn;
  act.player = currentPlayer; 
  humanAction(act); 
  update(); 
}

void GLDrawer::draw () { 
  paintGL(); 
}

int GLDrawer::assignTextureIndex () {
  int ret = assignedTextures;
  assignedTextures++;
  return ret; 
}

int GLDrawer::loadTexture (std::string fname, QColor backup) {
  QImage b;
  if (!b.load(fname.c_str())) {
    b = QImage(32, 32, QImage::Format_RGB888);
    b.fill(backup.rgb()); 
  }
  
  QImage t = QGLWidget::convertToGLFormat(b);

  /*
  QRgb pix = t.pixel(0, 0);
  Logger::logStream(DebugStartup) << "Loading " <<  fname << " : "
				  << qRed(pix) << " "
				  << qGreen(pix) << " "
				  << qBlue(pix) << " "
				  << qAlpha(pix) << "\n";
  */

  int index = assignTextureIndex(); 
  glBindTexture(GL_TEXTURE_2D, textureIDs[index]); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.width(), t.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.bits());

  return index; 
}

/*
int GLDrawer::loadTexture (int texName, QColor backup, string fname) {
  QImage b;
  if (!b.load(fname.c_str())) {
    b = QImage(32, 32, QImage::Format_RGB888);
    b.fill(backup.rgb()); 
  }
  
  QImage t = QGLWidget::convertToGLFormat(b);
  glBindTexture(GL_TEXTURE_2D, texName); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.width(), t.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.bits());
}
*/

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
  glGenTextures(numTextures, textureIDs);
  //const char* names[NoTerrain] = {"mountain.bmp", "hill.bmp", "gfx\\grass.png", "forest.bmp", "ocean.bmp"};
  //QColor colours[NoTerrain] = {Qt::gray, Qt::lightGray, Qt::yellow, Qt::green, Qt::blue};
  //terrainTextureIndices = new int[NoTerrain];
  //zoneTextureIndices = new int[1]; 
  //for (int i = 0; i < NoTerrain; ++i) {
  //terrainTextureIndices[i] = i;
  //loadTexture(names[i], colours[i], terrainTextureIndices[i]); 
  //}
  //zoneTextureIndices[0] = terrainTextureIndices[2]; 

  

  castleTextureIndices = new int[4];
  castleTextureIndices[0] = loadTexture("gfx\\castle1.png", Qt::red);  
  castleTextureIndices[1] = loadTexture("gfx\\flag1.png", Qt::red);
  castleTextureIndices[2] = loadTexture("gfx\\flag2.png", Qt::red);
  castleTextureIndices[3] = loadTexture("gfx\\flagstone3.png", Qt::red); 
  
  knightTextureIndices = new int[1];
  knightTextureIndices[0] = loadTexture("gfx\\kni1.png", Qt::red); 
  
  bool isgood = getGLExtensionFunctions().resolve(this->context()); 
  if (!isgood) Logger::logStream(Logger::Debug) << "Some GL functions not found.\n";
  if (!getGLExtensionFunctions().openGL15Supported()) Logger::logStream(Logger::Debug) << "Could not find OpenGL 1.5 support.\n";

  /*
  GLint maxtex = -1;
  glGetIntegerv(GL_MAX_TEXTURE_COORDS, &maxtex);
  Logger::logStream(Logger::Debug) << "Max textures: " << ((int) maxtex) << ".\n";
  if (!QGLFormat::openGLVersionFlags() & QGLFormat::OpenGL_Version_1_5) Logger::logStream(Logger::Debug) << "Format could not find OpenGL 1.5 support.\n";
  Logger::logStream(Logger::Debug) << "Function pointer: "
				   << ((int) glActiveTexture) << " "
				   << ((int) glMultiTexCoord2f) << " "   
				   << "\n";
  */
}

void GLDrawer::resizeGL () {
  glViewport(0, 0, width(), height());
}

void GLDrawer::paintGL () {
  //Logger::logStream(Logger::Debug) << "Here\n";
  //glClearColor(0.0, 1.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-0.75*zoomLevel, 0.75*zoomLevel, -0.5*zoomLevel, 0.5*zoomLevel, 0.25*zoomLevel, 2*zoomLevel);
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
  //Logger::logStream(Logger::Debug) << "Entering WarfareWindow::update.\n"; 
  hexDrawer->updateGL();
  selDrawer->draw(); 
  QMainWindow::update();
}

void WarfareWindow::wheelEvent (QWheelEvent* event) {
  hexDrawer->zoom(event->delta());
  update(); 
}

void WarfareWindow::selectObject () {
  if (selectedHex) {
    selDrawer->setSelected(selectedHex->getGraphicsInfo());
    farmInterface->setFarm(selectedHex->getFarm());
    farmInterface->show(); 
  }
  else farmInterface->hide();

  if (selectedVertex) {
    selDrawer->setSelected(selectedVertex->getGraphicsInfo());
    MilUnit* unit = selectedVertex->getUnit(0);    
    if ((unit) && (unit->getOwner() == currentPlayer)) {
      unitInterface->setUnit(unit); 
      unitInterface->show();
    }
    else unitInterface->hide(); 
  }
  else unitInterface->hide();
  
  if (selectedLine) {
    Castle* castle = selectedLine->getCastle();
    if ((castle) && (castle->getOwner() == currentPlayer)) {
      castleInterface->setCastle(castle);
      castleInterface->show();
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
      act.player = currentPlayer;
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
  act.player = currentPlayer;
  
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
      if (currentPlayer != selectedVertex->getUnit(0)->getOwner()) {
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
      if (selectedLine->getCastle()->getOwner() != currentPlayer) {
	Logger::logStream(Logger::Warning) << "Not your castle, cannot mobilise.\n"; 
	return;
      }
      if (0 == selectedLine->getCastle()->numGarrison()) {
	Logger::logStream(Logger::Warning) << "Empty castle, cannot be mobilised.\n"; 
	return;
      }
      if ((0 < clickedVertex->numUnits()) && (clickedVertex->getUnit(0)->getOwner() == currentPlayer)) {
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
      if (currentPlayer != selectedVertex->getUnit(0)->getOwner()) {
	Logger::logStream(Logger::Warning) << "Not your unit.\n";
	return; 
      }
      if (NoVertex == selectedVertex->getDirection(clickedVertex)) {
	Logger::logStream(Logger::Warning) << "Not adjacent.\n";
	return; 
      }
      if ((0 < clickedVertex->numUnits()) && (clickedVertex->getUnit(0)->getOwner() == currentPlayer)) {
      	Logger::logStream(Logger::Warning) << "Friendly unit is in the way.\n"; 
	return;
      }
      Line* middle = selectedVertex->getLine(clickedVertex);
      if (!middle) {
	Logger::logStream(Logger::Warning) << "Not adjacent.\n";
	return; 
      }
      if ((middle->getCastle()) && (middle->getCastle()->getOwner() != currentPlayer)) {
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
      if (currentPlayer != selectedVertex->getUnit(0)->getOwner()) {
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
      if (currentPlayer != selectedVertex->getUnit(0)->getOwner()) {
	Logger::logStream(Logger::Warning) << "Not your unit.\n"; 
	return; 
      }
      act.cease = clickedLine;
      act.start = selectedVertex;
      if (clickedLine->getCastle()->getOwner() == currentPlayer) {
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
      if (clickedLine->getCastle()->getOwner() != currentPlayer) {
	Logger::logStream(Logger::Warning) << "Not your castle, cannot reinforce.\n";
	return; 
      }
      if ((0 < clickedLine->oneEnd()->numUnits()) && (currentPlayer != clickedLine->oneEnd()->getUnit(0)->getOwner()) &&
	  (0 < clickedLine->twoEnd()->numUnits()) && (currentPlayer != clickedLine->twoEnd()->getUnit(0)->getOwner())) {
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
    hexDrawer->rotate(-0.01);
    break;

  case Qt::Key_Right:
    hexDrawer->rotate(0.01);
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

  for (Player::Iterator pl = Player::begin(); pl != Player::end(); ++pl) {
    if ((*pl)->turnEnded()) continue;
    return false; 
  }
  
  return true; 
}

void WarfareWindow::endOfTurn () {
  currentGame->endOfTurn();
  for (Player::Iterator pl = Player::begin(); pl != Player::end(); ++pl) {
    (*pl)->newTurn(); 
  }
}

Player* WarfareWindow::gameOver () {
  set<Player*> stillHaveCastles;
  for (Line::Iterator lin = Line::begin(); lin != Line::end(); ++lin) {
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
  
  currentPlayer = Player::nextPlayer(currentPlayer);
  runNonHumans();
  selectObject();
}

void WarfareWindow::runNonHumans () {
  while (!currentPlayer->isHuman()) {
    Logger::logStream(Logger::Debug) << currentPlayer->getDisplayName() << "\n";
    currentPlayer->getAction();
    //currentPlayer->finished(); 
    if (turnEnded()) endOfTurn(); 
    currentPlayer = Player::nextPlayer(currentPlayer);
    Logger::logStream(Logger::Debug) << currentPlayer->getDisplayName() << "\n";
  }
}

void WarfareWindow::message (QString m) {
  textWindow->appendPlainText(m); 
}

void WarfareWindow::initialiseColours () {
  hexDrawer->setFixedSize(899, 600);
  hexDrawer->setFixedSize(900, 600);
  selDrawer->setFixedSize(220, 300);
  for (Player::Iterator p = Player::begin(); p != Player::end(); ++p) {
    hexDrawer->assignColour(*p); 
  }
  hexDrawer->loadSprites(); 
}

void WarfareWindow::clearGame () {
    if (currentGame) {
    delete currentGame;
    currentGame = 0;
    currentPlayer = 0;
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
