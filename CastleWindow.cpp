#include "CastleWindow.hh"
#include "RiderGame.hh" 
#include <QPainter>
#include <QVector2D> 
#include "glextensions.h"
#include <QGLShaderProgram>
#include <cassert> 
#include "PopUnit.hh"
#include "MilUnit.hh" 
#include "Hex.hh"
#include "Logger.hh" 
#include "Object.hh"
#include "ThreeDSprite.hh" 

HexDrawer::HexDrawer (QWidget* p)
  : parent(p)
  , translateX(0)
  , translateY(0)
  , zoomLevel(4)
  , azimuth(0)
  , radial(0)
{}

SelectedDrawer::SelectedDrawer (QWidget* p) : QLabel(p) {}

void HexDrawer::azimate (double amount) {
  azimuth += amount;
  if (azimuth < 0) azimuth = 0;
  if (azimuth > 1.0471) azimuth = 1.0471; // 60 degrees. 
}

void HexDrawer::zoom (int delta) {
  if (delta > 0) {
    zoomLevel /= 2;
    if (zoomLevel < 2) zoomLevel = 2;
  }
  else {
    zoomLevel *= 2;
    if (zoomLevel > 64) zoomLevel = 64; 
  }
}

void SelectedDrawer::setSelected (Hex* s) {
  if (0 == s) {
    setText("");
    return;
  }
  QString nText;
  QTextStream(&nText) << "Hex " << s->getName().c_str() << "\n"
		      << "Owner: " << (s->getOwner() ? s->getOwner()->getDisplayName().c_str() : "None") << "\n"
  		      << "Devastation: " << s->getDevastation() << "\n";
  setText(nText); 
}

void SelectedDrawer::setSelected (Vertex* s) {
  if (0 == s) {
    setText("");
    return;
  }
  QString nText;
  QTextStream str(&nText);
  str << "Vertex: " << s->getName().c_str() << "\n";
  if (0 < s->numUnits()) {
    MilUnit* unit = s->getUnit(0); 
    str << "Unit: \n"
	<< "  Owner: " << unit->getOwner()->getDisplayName().c_str() << "\n"
	<< "  Status: " << (unit->weakened() ? "Weak" : "Strong") << "\n"; 
  }
  setText(nText); 
}

void SelectedDrawer::setSelected (Line* s) {
  if (0 == s) {
    setText("");
    return;
  }  
  QString nText;
  QTextStream str(&nText);
  str << "Line: " << s->getName().c_str() << "\n";
  if (0 < s->getCastle()) {
    Castle* castle = s->getCastle();
    str << "Castle: \n"
	<< "  Owner: " << castle->getOwner()->getDisplayName().c_str() << "\n"
	<< "  Garrison: " << castle->numGarrison() << "\n"
	<< "  Recruited: " << castle->getRecruitState() << " / " << Castle::maxRecruits << "\n"; 
  }
  setText(nText); 
}

GLDrawer::GLDrawer (QWidget* p)
  : HexDrawer(p)
  , QGLWidget(p)
  , hexes(0)
  , vexes(0)
  , lines(0)
  , numTextures(100)
  , cSprite(0)
  , kSprite(0)
{
  textureIDs = new GLuint[numTextures];
  errors = new int[100]; 
  setFixedSize(900, 600);
  move(230, 30);
  show();
  updateGL(); 
}

void GLDrawer::clearColours () {
  colourmap.clear();
}

void GLDrawer::setTranslate (int x, int y) { 
  translateX -= zoomLevel*(x*cos(radial) + y*sin(radial));
  translateY -= zoomLevel*(y*cos(radial) - x*sin(radial)); 
}

void GLDrawer::drawLine (Line* dat) {
  Castle* curr = dat->getCastle();
  if (!curr) return;
  
  Hex* one = curr->getSupport();
  std::pair<int, int> pos = one->getPos(); 
  std::pair<double, double> coords1 = hexCenter(pos.first, pos.second);
  pos = one->getPos(one->getDirection(dat));
  std::pair<double, double> coords2 = hexCenter(pos.first, pos.second);
  coords1.first += 0.4*(coords2.first - coords1.first); 
  coords1.second += 0.4*(coords2.second - coords1.second); 

  std::vector<int> texts;
  for (int i = 0; i < curr->numGarrison(); ++i) {
    texts.push_back(playerToTextureMap[curr->getOwner()]); 
  }

  double angle = 0;
  switch (one->getDirection(dat)) {
  case Hex::None:
  case Hex::North:
  case Hex::South:
  default: 
    break;
  case Hex::NorthWest: angle = 30; break; 
  case Hex::SouthWest: angle = 60; break;
  case Hex::NorthEast: angle = -30; break;
  case Hex::SouthEast: angle = -60; break; 
  }
  
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glTranslated(coords1.first, coords1.second, 0);
  glRotated(angle, 0, 0, 1); 
  glBindTexture(GL_TEXTURE_2D, textureIDs[castleTextureIndices[3]]); 
  cSprite->draw(texts);
  glPopMatrix(); 
}

void GLDrawer::drawVertex (Vertex* dat) {
  if (0 == dat->numUnits()) return; 
  std::pair<double, double> center = vertexCenter(dat);

  std::vector<int> texts;
  texts.push_back(playerToTextureMap[dat->getUnit(0)->getOwner()]);
  texts.push_back(playerToTextureMap[dat->getUnit(0)->getOwner()]); 


  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glTranslated(center.first, center.second, 0);
  double angle = 0;
  switch (dat->getUnit(0)->getRear()) {
  case Hex::Left:
  case Hex::NoVertex:
  default:
    break;
  case Hex::Right     : angle = 180; break;
  case Hex::RightDown : angle = 240; break;
  case Hex::LeftDown  : angle = -60; break;
  case Hex::RightUp   : angle = 120; break;
  case Hex::LeftUp    : angle =  60; break;
  }
  
  glBindTexture(GL_TEXTURE_2D, 0);
  if (dat->getUnit(0)->weakened()) {
    glRotated(angle, 0, 0, 1); 
    kSprite->draw(texts);
  }
  else {
    glTranslated(0, 0.1, 0);
    glRotated(angle, 0, 0, 1); 
    kSprite->draw(texts);
    glRotated(-angle, 0, 0, 1); 
    glTranslated(0, -0.2, 0);
    glRotated(angle, 0, 0, 1); 
    glBindTexture(GL_TEXTURE_2D, 0);
    kSprite->draw(texts);
  }
  glPopMatrix(); 
}

void GLDrawer::drawHex (Hex* dat) {

}

ThreeDSprite* GLDrawer::makeSprite (Object* info) {
  std::string castleFile = info->safeGetString("filename", "nosuchbeast");
  assert(castleFile != "nosuchbeast");
  std::vector<std::string> specs;
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
  double* curr = new double[4];
  curr[0] = 0;
  curr[1] = 0;
  curr[2] = 0;
  curr[3] = 1;
  curr[colourmap.size()] = 1;
  colourmap[p] = curr;

  std::string pName = "gfx/" + p->getName() + ".png";
  GLuint texid; 
  glGenTextures(1, &texid);
  loadTexture(texid, Qt::red, pName);
  playerToTextureMap[p] = texid; 
}

bool GLDrawer::intersect (double line1x1, double line1y1, double line1x2, double line1y2,
			  double line2x1, double line2y1, double line2x2, double line2y2) {

  double line1A = line1y2 - line1y1;
  double line1B = line1x1 - line1x2; 
  double line1C = line1A*line1x1 + line1B*line1y1;

  double line2A = line2y2 - line2y1;
  double line2B = line2x1 - line2x2; 
  double line2C = line2A*line2x1 + line2B*line2y1;

  double det = line1A*line2B - line1B*line2A;
  if (fabs(det) < 0.00001) return false; // Parallel lines

  double xinter = (line2B*line1C - line1B*line2C)/det;
  static const double tol = 0.0001;
  if (xinter < std::min(line1x1, line1x2) - tol) return false;  // Small tolerance accounts for roundoff error
  if (xinter > std::max(line1x1, line1x2) + tol) return false;
  if (xinter < std::min(line2x1, line2x2) - tol) return false;
  if (xinter > std::max(line2x1, line2x2) + tol) return false;
  double yinter = (line1A*line2C - line2A*line1C)/det;
  if (yinter < std::min(line1y1, line1y2) - tol) return false;
  if (yinter > std::max(line1y1, line1y2) + tol) return false;
  if (yinter < std::min(line2y1, line2y2) - tol) return false;
  if (yinter > std::max(line2y1, line2y2) + tol) return false;

  /*
  Logger::logStream(Logger::Game) << "Intersection at ("
				  << xinter << ", " << yinter << ") ("
				  << line1x1 << " " << line1y1 << " " << line1x2 << " " << line1y2 << ") ("
    				  << line2x1 << " " << line2y1 << " " << line2x2 << " " << line2y2 << ") "
				  << "\n";
  */
  return true;
}

GLDrawer::Triangle GLDrawer::vertexTriangle (Vertex* vex) const {
  Triangle ret; 
  Hex* corner = 0;
  int i = 0; 
  while (!corner) {
    corner = vex->getHex(i++);
    if (i >= Hex::None) break;
  }
  assert(corner);
  Hex::Vertices dir = corner->getDirection(vex);
  std::pair<int, int> pos1 = corner->getPos();
  ret.get<0>() = vertexCoords(pos1.first, pos1.second, dir);
  std::pair<int, int> temp(0, 0); 
  switch (dir) {
  case Hex::LeftUp:
    temp = Hex::getNeighbourCoordinates(pos1, Hex::North);
    ret.get<1>() = vertexCoords(temp.first, temp.second, Hex::LeftDown);
    temp = Hex::getNeighbourCoordinates(pos1, Hex::NorthWest);
    ret.get<2>() = vertexCoords(temp.first, temp.second, Hex::Right);
    break; 
  case Hex::RightUp:
    temp = Hex::getNeighbourCoordinates(pos1, Hex::North);
    ret.get<1>() = vertexCoords(temp.first, temp.second, Hex::RightDown);
    temp = Hex::getNeighbourCoordinates(pos1, Hex::NorthEast);
    ret.get<2>() = vertexCoords(temp.first, temp.second, Hex::Left); 		     
    break; 					  
  case Hex::Right:
    temp = Hex::getNeighbourCoordinates(pos1, Hex::NorthEast);
    ret.get<1>() = vertexCoords(temp.first, temp.second, Hex::LeftDown); 		     
    temp = Hex::getNeighbourCoordinates(pos1, Hex::SouthEast);
    ret.get<2>() = vertexCoords(temp.first, temp.second, Hex::LeftUp); 
    break; 					  
  case Hex::RightDown:
    temp = Hex::getNeighbourCoordinates(pos1, Hex::South);
    ret.get<1>() = vertexCoords(temp.first, temp.second, Hex::RightUp); 
    temp = Hex::getNeighbourCoordinates(pos1, Hex::SouthEast);
    ret.get<2>() = vertexCoords(temp.first, temp.second, Hex::Left); 
    break; 					  
  case Hex::LeftDown:
    temp = Hex::getNeighbourCoordinates(pos1, Hex::South);
    ret.get<1>() = vertexCoords(temp.first, temp.second, Hex::LeftUp); 		     
    temp = Hex::getNeighbourCoordinates(pos1, Hex::SouthWest);
    ret.get<2>() = vertexCoords(temp.first, temp.second, Hex::Right);
    break; 					  
  case Hex::Left:
    temp = Hex::getNeighbourCoordinates(pos1, Hex::NorthWest);
    ret.get<1>() = vertexCoords(temp.first, temp.second, Hex::RightDown); 			     
    temp = Hex::getNeighbourCoordinates(pos1, Hex::SouthWest);
    ret.get<2>() = vertexCoords(temp.first, temp.second, Hex::RightUp); 
    break; 					  
  case Hex::None:
  default:
    assert(false); 
  }

  return ret; 
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
  if (!hexes) return 0;
  if (x < 0) return 0;
  if (y < 0) return 0;
  if (x > width()) return 0;
  if (y > height()) return 0; 
  convertToOGL(x, y); 
  
  for (Hex::Iterator hex = hexes->start; hex != hexes->final; ++hex) {
    int xidx = (*hex)->getPos().first;
    int yidx = (*hex)->getPos().second; 
    int intersections = 0;
    std::pair<double, double> center = vertexCoords(xidx, yidx, Hex::NoVertex);
    std::pair<double, double> radius = vertexCoords(xidx, yidx, Hex::LeftUp);
    if (pow(x - center.first, 2) + pow(y - center.second, 2) > pow(radius.first - center.first, 2) + pow(radius.second - center.second, 2)) continue; 
    for (int i = Hex::RightUp; i < Hex::NoVertex; ++i) {
      std::pair<double, double> coords1 = vertexCoords(xidx, yidx, Hex::convertToVertex(i-1));
      std::pair<double, double> coords2 = vertexCoords(xidx, yidx, Hex::convertToVertex(i));

      if (!intersect(x, y, x+10, y+10, coords1.first, coords1.second, coords2.first, coords2.second)) continue;
      intersections++;
      //Logger::logStream(Logger::Game) << "Intersect (" << xidx << ", " << yidx << ") " << i-1 << " " << i << ".\n"; 
    }
    std::pair<double, double> coords1 = vertexCoords(xidx, yidx, Hex::LeftUp); 
    std::pair<double, double> coords2 = vertexCoords(xidx, yidx, Hex::Left); 
    if (intersect(x, y, x+10, y+10, coords1.first, coords1.second, coords2.first, coords2.second)) intersections++;
    if (0 == intersections % 2) continue;

    return (*hex); 
  }
  //Logger::logStream(Logger::Game) << "No hex at " << x << " " << y << ".\n"; 
  return 0; 
}

Line* GLDrawer::findLine (double x, double y) {
  if (!lines) return 0;
  if (x < 0) return 0;
  if (y < 0) return 0;
  if (x > width()) return 0;
  if (y > height()) return 0; 
  convertToOGL(x, y); 
  
  for (Line::Iterator lin = lines->start; lin != lines->final; ++lin) {
    Hex* one = (*lin)->oneHex();
    std::pair<int, int> pos = one->getPos();
    Hex::Vertices dir11 = one->getDirection((*lin)->oneEnd());
    Hex::Vertices dir12 = one->getDirection((*lin)->twoEnd());
    std::pair<double, double> coords11 = vertexCoords(pos.first, pos.second, dir11);    
    std::pair<double, double> coords12 = vertexCoords(pos.first, pos.second, dir12);
    std::pair<int, int> po2 = Hex::getNeighbourCoordinates(pos, one->getDirection(*lin)); 
    std::pair<double, double> coords21 = vertexCoords(po2.first, po2.second, Hex::oppositeVertex(dir11));    
    std::pair<double, double> coords22 = vertexCoords(po2.first, po2.second, Hex::oppositeVertex(dir12));    

    if (x < std::min(std::min(coords11.first, coords12.first), std::min(coords21.first, coords22.first))) continue;
    if (x > std::max(std::max(coords11.first, coords12.first), std::max(coords21.first, coords22.first))) continue;
    if (y < std::min(std::min(coords11.second, coords12.second), std::min(coords21.second, coords22.second))) continue;
    if (y > std::max(std::max(coords11.second, coords12.second), std::max(coords21.second, coords22.second))) continue;    
    
    int intersections = 0;
    int mask = 0; 
    if (intersect(x, y, x+10, y+10, coords11.first, coords11.second, coords12.first, coords12.second)) {intersections++; mask += 1;}
    if (intersect(x, y, x+10, y+10, coords12.first, coords12.second, coords21.first, coords21.second)) {intersections++; mask += 2;}
    if (intersect(x, y, x+10, y+10, coords21.first, coords21.second, coords22.first, coords22.second)) {intersections++; mask += 4;}
    if (intersect(x, y, x+10, y+10, coords22.first, coords22.second, coords11.first, coords11.second)) {intersections++; mask += 8;}
    if (0 == intersections % 2) continue;
    /*
    Logger::logStream(Logger::Game) << "Clicked line at ("
				    << coords11.first << ", " << coords11.second << ", " << coords12.first << ", " << coords12.second << ") ("
      				    << coords12.first << ", " << coords12.second << ", " << coords21.first << ", " << coords21.second << ") ("
      				    << coords21.first << ", " << coords21.second << ", " << coords22.first << ", " << coords22.second << ") ("
      				    << coords22.first << ", " << coords22.second << ", " << coords11.first << ", " << coords11.second << ") ("
				    << x << ", " << y << ") "
				    << mask 
				    << "\n"; 
    */
    return (*lin); 
  }
  return 0;
}



Vertex* GLDrawer::findVertex (double x, double y) {
  if (!vexes) return 0;
  if (x < 0) return 0;
  if (y < 0) return 0;
  if (x > width()) return 0;
  if (y > height()) return 0; 
  convertToOGL(x, y); 
  
  for (Vertex::Iterator vex = vexes->start; vex != vexes->final; ++vex) {
    Triangle tri = vertexTriangle(*vex);
    if (x < std::min(tri.get<0>().first, std::min(tri.get<1>().first, tri.get<2>().first))) continue;
    if (x > std::max(tri.get<0>().first, std::max(tri.get<1>().first, tri.get<2>().first))) continue;
    if (y < std::min(tri.get<0>().second, std::min(tri.get<1>().second, tri.get<2>().second))) continue;
    if (y > std::max(tri.get<0>().second, std::max(tri.get<1>().second, tri.get<2>().second))) continue;
    int intersections = 0;
    if (intersect(x, y, x+10, y+10, tri.get<0>().first, tri.get<0>().second, tri.get<1>().first, tri.get<1>().second)) intersections++;
    if (intersect(x, y, x+10, y+10, tri.get<0>().first, tri.get<0>().second, tri.get<2>().first, tri.get<2>().second)) intersections++;
    if (intersect(x, y, x+10, y+10, tri.get<2>().first, tri.get<2>().second, tri.get<1>().first, tri.get<1>().second)) intersections++;    
    if (0 == intersections % 2) continue;
    return (*vex); 
  }
  return 0;
}

int main (int argc, char** argv) {
  Logger::createStream(Logger::Debug);
  Logger::createStream(Logger::Trace);
  Logger::createStream(Logger::Game);
  Logger::createStream(Logger::Warning);
  Logger::createStream(Logger::Error);
  
  QApplication industryApp(argc, argv);

  QDesktopWidget* desk = QApplication::desktop();
  QRect scr = desk->availableGeometry();
  WarfareWindow window;
  window.show();

  window.textWindow = new QPlainTextEdit(&window);
  window.textWindow->setFixedSize(900, 140);
  window.textWindow->move(230, 640);
  window.textWindow->show(); 

  QObject::connect(&(Logger::logStream(Logger::Debug)),   SIGNAL(message(QString)), &window, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Trace)),   SIGNAL(message(QString)), &window, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Game)),    SIGNAL(message(QString)), &window, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Warning)), SIGNAL(message(QString)), &window, SLOT(message(QString)));
  QObject::connect(&(Logger::logStream(Logger::Error)),   SIGNAL(message(QString)), &window, SLOT(message(QString))); 
  
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

  QObject::connect(quit, SIGNAL(triggered()), &window, SLOT(close())); 
  QObject::connect(newGame, SIGNAL(triggered()), &window, SLOT(newGame()));
  QObject::connect(loadGame, SIGNAL(triggered()), &window, SLOT(loadGame()));
  QObject::connect(saveGame, SIGNAL(triggered()), &window, SLOT(saveGame()));   
  QObject::connect(endTurn, SIGNAL(triggered()), &window, SLOT(endTurn())); 

  QPushButton* endTurnButton = new QPushButton("&End turn", &window);
  endTurnButton->setFixedSize(60, 30);
  endTurnButton->move(90, 300);
  QObject::connect(endTurnButton, SIGNAL(clicked()), &window, SLOT(endTurn()));
  QObject::connect(newGame, SIGNAL(triggered()), endTurnButton, SLOT(show()));

  window.show();

  if (argc > 1) window.newGame(argv[1]); 
  
  return industryApp.exec();  
}


WarfareWindow::WarfareWindow (QWidget* parent)
  : QMainWindow(parent)
  , selectedHex(0)
  , selectedLine(0)
  , selectedVertex(0)
{
  hexDrawer = new GLDrawer(this);
  selDrawer = new SelectedDrawer(this);
  selDrawer->move(15, 30);
  selDrawer->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  selDrawer->show(); 
  currentGame = 0;
}

WarfareWindow::~WarfareWindow () {}

void WarfareWindow::newGame () {
  QString filename = QFileDialog::getOpenFileName(this, tr("Select file"), QString("./scenarios/"), QString("*.txt"));
  std::string fn(filename.toAscii().data());
  if (fn == "") return;
  newGame(fn); 
}

void WarfareWindow::newGame (std::string fname) {
  clearGame(); 
  currentGame = WarfareGame::createGame(fname, currentPlayer);
  initialiseColours();
  runNonHumans();
  update();
}

void WarfareWindow::loadGame () {
  QString filename = QFileDialog::getOpenFileName(this, tr("Select file"), QString("./savegames/"), QString("*.txt"));
  std::string fn(filename.toAscii().data());
  if (fn == "") return;

  clearGame();
  currentGame = WarfareGame::createGame(fn, currentPlayer);
  initialiseColours();
  runNonHumans();
  update();
}

void WarfareWindow::saveGame () {
  QString filename = QFileDialog::getSaveFileName(this, tr("Select file"), QString("./savegames/"), QString("*.txt"));
  std::string fn(filename.toAscii().data());
  if (fn == "") return;

  WarfareGame::saveGame(fn, currentPlayer);
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

void GLDrawer::draw (DrawInfo<Hex>* h, DrawInfo<Vertex>* v, DrawInfo<Line>* l) {
  hexes = h;
  vexes = v;
  lines = l; 
  paintGL(); 
}

void GLDrawer::loadTexture (const char* fname, QColor backup, int idx) {
  QImage b;
  if (!b.load(fname)) {
    b = QImage(32, 32, QImage::Format_RGB888);
    b.fill(backup.rgb()); 
  }
  
  QImage t = QGLWidget::convertToGLFormat(b);
  glBindTexture(GL_TEXTURE_2D, textureIDs[idx]); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexImage2D(GL_TEXTURE_2D, 0, 4, t.width(), t.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.bits());
}

void GLDrawer::loadTexture (int texName, QColor backup, std::string fname) {
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
  glTexImage2D(GL_TEXTURE_2D, 0, 4, t.width(), t.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.bits());
}

void GLDrawer::initializeGL () {
  makeCurrent(); 
  glEnable(GL_DEPTH_TEST);
  glClearDepth(1000);
  glClearColor(0.0, 0.0, 0.0, 0.0);

  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-3, 3, -2, 2, 4, 6);
  glViewport(0, 0, width(), height());
  glMatrixMode(GL_MODELVIEW); 
  glLoadIdentity();             
  //glTranslatef (0.0, 0.0, -5.0);
  gluLookAt(0.0, 0.0, zoomLevel,
	    0.0, 0.0, 0.0,
	    0.0, -1.0, 0.0); 
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  
  glGenTextures(numTextures, textureIDs);
  const char* names[Hex::Unknown] = {"mountain.bmp", "hill.bmp", "plain.bmp", "forest.bmp", "ocean.bmp"};
  QColor colours[Hex::Unknown] = {Qt::gray, Qt::lightGray, Qt::yellow, Qt::green, Qt::blue};
  terrainTextureIndices = new int[Hex::Unknown]; 
  for (int i = 0; i < Hex::Unknown; ++i) {
    terrainTextureIndices[i] = i;
    loadTexture(names[i], colours[i], terrainTextureIndices[i]); 
  }

  castleTextureIndices = new int[4];
  castleTextureIndices[0] = 1+Hex::Unknown;
  loadTexture("gfx\\castle1.png", Qt::red, castleTextureIndices[0]);  
  castleTextureIndices[1] = 2+Hex::Unknown;
  loadTexture("gfx\\flag1.png", Qt::red, castleTextureIndices[1]); 
  castleTextureIndices[2] = 3+Hex::Unknown;
  loadTexture("gfx\\flag2.png", Qt::red, castleTextureIndices[2]); 
  castleTextureIndices[3] = 5+Hex::Unknown;
  loadTexture("gfx\\flagstone3.png", Qt::red, castleTextureIndices[3]); 
  
  knightTextureIndices = new int[1];
  knightTextureIndices[0] = 4+Hex::Unknown;
  loadTexture("gfx\\kni1.png", Qt::red, knightTextureIndices[0]); 
  
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

std::pair<double, double> GLDrawer::vertexCenter (Vertex* dat) const {
  Hex* hex = 0;
  for (Vertex::HexIterator i = dat->beginHexes(); i != dat->endHexes(); ++i) {
    if (!(*i)) continue;
    hex = (*i);
    break; 
  }
  assert(hex);
  Hex::Direction one = Hex::None;
  Hex::Direction two = Hex::None;
  switch (hex->getDirection(dat)) {
  case Hex::None:
  default: 
    assert(false);
    break;
  case Hex::Right:
    one = Hex::NorthEast;
    two = Hex::SouthEast;
    break;
  case Hex::Left:
    one = Hex::NorthWest;
    two = Hex::SouthWest;
    break;
  case Hex::LeftUp:
    one = Hex::NorthWest;
    two = Hex::North;
    break;
  case Hex::RightUp:
    one = Hex::NorthEast;
    two = Hex::North;
    break;
  case Hex::RightDown:
    one = Hex::SouthEast;
    two = Hex::South;
    break;
  case Hex::LeftDown:
    one = Hex::SouthWest;
    two = Hex::South;
    break; 
  }

  std::pair<int, int> pos = hex->getPos();
  std::pair<double, double> ret = hexCenter(pos.first, pos.second);
  pos = hex->getPos(one);
  std::pair<double, double> c2 = hexCenter(pos.first, pos.second);
  pos = hex->getPos(two);
  std::pair<double, double> c3 = hexCenter(pos.first, pos.second);

  ret.first  += c2.first  + c3.first;  ret.first  *= 0.333;
  ret.second += c2.second + c3.second; ret.second *= 0.333; 
    
  return ret; 
}

std::pair<double, double> GLDrawer::hexCenter (int x, int y) const {
  static const double xIncrement = 1.0;
  static const double yIncrement = sqrt(pow(xIncrement, 2) - pow(0.5*xIncrement, 2));
  static const double xSeparation = 0.8;
  static const double ySeparation = xSeparation/sqrt(3); 
  
  std::pair<double, double> ret(0, 0); 
  ret.first = (1.5 + xSeparation) * xIncrement * x;
  ret.second = (1.0 + ySeparation) * yIncrement*(2*y+ (x >= 0 ? x%2 : -x%2));
  return ret; 
}

std::pair<double, double> GLDrawer::vertexCoords (int x, int y, Hex::Vertices dir) const {
  static const double xIncrement = 1.0;
  static const double yIncrement = sqrt(pow(xIncrement, 2) - pow(0.5*xIncrement, 2));
  //static const double xSeparation = 0.8;
  //static const double ySeparation = xSeparation/sqrt(3); 
  std::pair<double, double> ret = hexCenter(x, y);
  //ret.first = 0.01*translateX + (1.5 + xSeparation) * xIncrement * x;
  //ret.second = -0.01*translateY + (1.0 + ySeparation) * yIncrement*(2*y+ (x >= 0 ? x%2 : -x%2));
  // Minus is due to difference in Qt and OpenGL coordinate systems. 
  // Also affects the signs in the y increments in the switch. 
  switch (dir) {
  case Hex::LeftUp:
    ret.first -= 0.5;
    ret.second -= yIncrement;
    break;
  case Hex::RightUp:
    ret.first += 0.5;
    ret.second -= yIncrement;
    break; 
  case Hex::Right:
    ret.first += 1.0;
    break;
  case Hex::Left:
    ret.first -= 1.0;
    break; 
  case Hex::RightDown:
    ret.first += 0.5;
    ret.second += yIncrement;
    break; 
  case Hex::LeftDown:
    ret.first -= 0.5;
    ret.second += yIncrement;
  case Hex::NoVertex: 
  default: break; 
  }
  
  return ret; 
}

void GLDrawer::paintGL () {
  //Logger::logStream(Logger::Debug) << "Here\n";
  //glClearColor(0.0, 1.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!hexes) return;
#ifdef STUPID 
  /*
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.5, 1.5, -1, 1, 0.5, 2);
  glViewport(0, 0, width(), height());
  glMatrixMode(GL_MODELVIEW);
  //glTranslatef(0, 0, -5);
  gluLookAt(0, 0, 1,
	    0, 0, 0,
	    0, 1, 0); 
  
  //glFrustum(-0.75*zoomLevel, 0.75*zoomLevel, -0.5*zoomLevel, 0.5*zoomLevel, 0.25*zoomLevel, 2*zoomLevel);
  //glMatrixMode(GL_MODELVIEW); 
  //glLoadIdentity();

  glColor3d(1.0, 1.0, 1.0);
  glBegin(GL_LINE_LOOP);
  glVertex3d(0, 0, 0);
  glVertex3d(1, 0, 0);
  glVertex3d(1, 1, 0);
  glVertex3d(0, 1, 0);
  glEnd();
  */  
  glBegin(GL_QUADS);
  glVertex3d(0, 0, 0);
  glVertex3d(0, 1, 0);
  glVertex3d(1, 1, 0);
  glVertex3d(1, 0, 0);

  glVertex3d(1, 1, 0);
  glVertex3d(1, 2, 0);
  glVertex3d(2, 2, 0);
  glVertex3d(2, 1, 0);

  //glVertex3d(0, 0, 0);
  //glVertex3d(0, 1, 0);
  //glVertex3d(0, 1, 1);
  //glVertex3d(0, 0, 1);
  glEnd();

  return;
#endif

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-0.75*zoomLevel, 0.75*zoomLevel, -0.5*zoomLevel, 0.5*zoomLevel, 0.25*zoomLevel, 2*zoomLevel);
  glMatrixMode(GL_MODELVIEW); 
  glLoadIdentity();
  
  gluLookAt(0.01*translateX+zoomLevel*sin(azimuth)*sin(radial), // Notice x-y switch, to make North point upwards with radial=0
	    0.01*translateY+zoomLevel*sin(azimuth)*cos(radial),
	    -zoomLevel*cos(azimuth), 
	    0.01*translateX, 0.01*translateY, 0.0,
	    //0, 0, 0, 
	    -cos(azimuth)*sin(radial), -cos(azimuth)*cos(radial), -sin(azimuth)); 

  //glBindTexture(GL_TEXTURE_2D, 0);
  glColor4d(0.0, 0.0, 0.0, 0.5);
  glBegin(GL_QUADS);
  
  glVertex3d(-1000, -1000, 0.01);
  glVertex3d( 1000, -1000, 0.01);
  glVertex3d( 1000,  1000, 0.01);
  glVertex3d(-1000,  1000, 0.01);
  /*
  glVertex3d(-1000, -1000, -0.01);
  glVertex3d(-1000,  1000, -0.01);
  glVertex3d( 1000,  1000, -0.01);
  glVertex3d( 1000, -1000, -0.01);
  
  */
  glEnd();

  glColor4d(1.0, 1.0, 1.0, 1.0);
  for (Line::Iterator lin = lines->start; lin != lines->final; ++lin) {
    if (!(*lin)->getCastle()) continue;
    drawLine(*lin);
  }
    
  static const double xTexCoords[8] = {0.50, 1.00, 0.75, 0.25, 0.00, 0.25, 0.75, 1.00};
  static const double yTexCoords[8] = {0.50, 0.50, 0.93, 0.93, 0.50, 0.07, 0.07, 0.50};
  
  for (Hex::Iterator hex = hexes->start; hex != hexes->final; ++hex) {
    int xidx = (*hex)->getPos().first;
    int yidx = (*hex)->getPos().second; 
    
    glColor4d(1.0, 1.0, 1.0, 1.0);
    std::pair<double, double> coords = vertexCoords(xidx, yidx, Hex::NoVertex);
    glBindTexture(GL_TEXTURE_2D, textureIDs[terrainTextureIndices[(*hex)->getType()]]);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2d(xTexCoords[0], yTexCoords[0]); glVertex3d(coords.first, coords.second, 0.0);
    coords = vertexCoords(xidx, yidx, Hex::Right);
    glTexCoord2d(xTexCoords[1], yTexCoords[1]); glVertex3d(coords.first, coords.second, 0.0);
    coords = vertexCoords(xidx, yidx, Hex::RightUp);
    glTexCoord2d(xTexCoords[2], yTexCoords[2]); glVertex3d(coords.first, coords.second, 0.0);
    coords = vertexCoords(xidx, yidx, Hex::LeftUp);
    glTexCoord2d(xTexCoords[3], yTexCoords[3]); glVertex3d(coords.first, coords.second, 0.0);
    coords = vertexCoords(xidx, yidx, Hex::Left);
    glTexCoord2d(xTexCoords[4], yTexCoords[4]); glVertex3d(coords.first, coords.second, 0.0);
    coords = vertexCoords(xidx, yidx, Hex::Hex::LeftDown);
    glTexCoord2d(xTexCoords[5], yTexCoords[5]); glVertex3d(coords.first, coords.second, 0.0);
    coords = vertexCoords(xidx, yidx, Hex::RightDown);
    glTexCoord2d(xTexCoords[6], yTexCoords[6]); glVertex3d(coords.first, coords.second, 0.0);
    coords = vertexCoords(xidx, yidx, Hex::Right);
    glTexCoord2d(xTexCoords[7], yTexCoords[7]); glVertex3d(coords.first, coords.second, 0.0);
    glEnd();

    /*    
    glColor3d(0.0, 1.0, 0.0);
    coords = vertexCoords(xidx, yidx, Hex::NoVertex);
    renderText(coords.first, coords.second, -0.1, (*hex)->toString().c_str());
    */
  }

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  for (Vertex::Iterator vex = vexes->start; vex != vexes->final; ++vex) {
    if (0 == (*vex)->numUnits()) continue;
    drawVertex(*vex);
#ifdef NOTDEF
    Triangle tri = vertexTriangle(*vex);
    QVector2D vtx1(tri.get<0>().first, tri.get<0>().second);
    QVector2D vtx2(tri.get<1>().first, tri.get<1>().second);
    QVector2D vtx3(tri.get<2>().first, tri.get<2>().second);
    QVector2D mid1 = (vtx1 + vtx3); mid1 *= 0.5;
    QVector2D mid2 = (vtx2 + vtx3); mid2 *= 0.5;
    QVector2D base = (vtx2 - vtx1);
    base.normalize();
    QVector2D prj1 = (vtx1 + (base * QVector2D::dotProduct(base, mid1 - vtx1)));
    QVector2D prj2 = (vtx1 + (base * QVector2D::dotProduct(base, mid2 - vtx1)));


    glBindTexture(GL_TEXTURE_2D, 0);
    glColor4dv(colourmap[(*vex)->getUnit(0)->getOwner()]); 
    glBegin(GL_QUADS);
    glVertex3d(prj1.x(), prj1.y(), 0);
    glVertex3d(prj2.x(), prj2.y(), 0);
    glVertex3d(mid2.x(), mid2.y(), 0);
    glVertex3d(mid1.x(), mid1.y(), 0);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, textureIDs[knightTextureIndices[0]]); 
    glBegin(GL_QUADS);
    glTexCoord2d(0.0, 0.0); glVertex3d(prj1.x(), prj1.y(), -0.01);
    glTexCoord2d(1.0, 0.0); glVertex3d(prj2.x(), prj2.y(), -0.01);
    glTexCoord2d(1.0, 1.0); glVertex3d(mid2.x(), mid2.y(), -0.01);
    glTexCoord2d(0.0, 1.0); glVertex3d(mid1.x(), mid1.y(), -0.01);
    glEnd();
#endif 
  }


  for (Line::Iterator lin = lines->start; lin != lines->final; ++lin) {
    if (!(*lin)->getCastle()) continue;
#ifdef NOTDEF
    {
    Hex* one = (*lin)->oneHex();
    std::pair<int, int> pos = one->getPos();
    Hex::Vertices dir11 = one->getDirection((*lin)->oneEnd());
    Hex::Vertices dir12 = one->getDirection((*lin)->twoEnd());
    std::pair<double, double> coords11 = vertexCoords(pos.first, pos.second, dir11);    
    std::pair<double, double> coords12 = vertexCoords(pos.first, pos.second, dir12);
    std::pair<int, int> po2 = Hex::getNeighbourCoordinates(pos, one->getDirection(*lin)); 
    std::pair<double, double> coords21 = vertexCoords(po2.first, po2.second, Hex::oppositeVertex(dir11));    
    std::pair<double, double> coords22 = vertexCoords(po2.first, po2.second, Hex::oppositeVertex(dir12));    
    glColor4d(1.0, 1.0, 1.0, 1.0);
    glBegin(GL_LINE_LOOP);
    glVertex3d(coords11.first, coords11.second, -0.001);
    glVertex3d(coords12.first, coords12.second, -0.001);
    glVertex3d(coords21.first, coords21.second, -0.001);
    glVertex3d(coords22.first, coords22.second, -0.001);
    glEnd();
    }

    Hex* one = (*lin)->getCastle()->getSupport();
    std::pair<int, int> pos = one->getPos();
    Hex::Vertices dir11 = one->getDirection((*lin)->oneEnd());
    Hex::Vertices dir12 = one->getDirection((*lin)->twoEnd());
    std::pair<double, double> coords11 = vertexCoords(pos.first, pos.second, dir11);    
    std::pair<double, double> coords12 = vertexCoords(pos.first, pos.second, dir12);
    std::pair<int, int> po2 = Hex::getNeighbourCoordinates(pos, one->getDirection(*lin)); 
    std::pair<double, double> coords21 = vertexCoords(po2.first, po2.second, Hex::oppositeVertex(dir11));    
    std::pair<double, double> coords22 = vertexCoords(po2.first, po2.second, Hex::oppositeVertex(dir12));    

    glColor4dv(colourmap[(*lin)->getCastle()->getOwner()]); 
    glBindTexture(GL_TEXTURE_2D, 0); 
    glBegin(GL_QUADS);
    glVertex3d(coords11.first, coords11.second, 0.001);
    glVertex3d(coords12.first, coords12.second, 0.001);
    glVertex3d(coords21.first, coords21.second, 0.001);
    glVertex3d(coords22.first, coords22.second, 0.001);
    glEnd();
    
    glColor4d(1.0, 1.0, 1.0, 0.0);
    for (int i = 0; i < 3; ++i) {
      if (3-i <= (*lin)->getCastle()->numGarrison()) break;
      glBindTexture(GL_TEXTURE_2D, textureIDs[castleTextureIndices[i]]); 
      glBegin(GL_QUADS);
      glTexCoord2d(0.0, 0.0); glVertex3d(coords11.first, coords11.second, -0.001*i);
      glTexCoord2d(1.0, 0.0); glVertex3d(coords12.first, coords12.second, -0.001*i);
      glTexCoord2d(1.0, 1.0); glVertex3d(coords21.first, coords21.second, -0.001*i);
      glTexCoord2d(0.0, 1.0); glVertex3d(coords22.first, coords22.second, -0.001*i);
      glEnd();
    }
    //glBindTexture(GL_TEXTURE_2D, textureIDs[castleTextureIndices[3]]);     
    //drawLine(*lin);
#endif 
  }

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); 

  
  
}

void WarfareWindow::paintEvent (QPaintEvent *event) {
  if (!currentGame) return;

  static DrawInfo<Hex> hexes;
  static DrawInfo<Vertex> vexes;
  static DrawInfo<Line> lines;

  hexes.start    = Hex::begin();
  hexes.final    = Hex::end(); 
  hexes.selected = selectedHex;

  vexes.start    = Vertex::begin();
  vexes.final    = Vertex::end();
  vexes.selected = selectedVertex;

  lines.start    = Line::begin();
  lines.final    = Line::end();
  lines.selected = selectedLine; 
  
  hexDrawer->draw(&hexes, &vexes, &lines); 
}

void WarfareWindow::update () {
  hexDrawer->updateGL(); 
  QMainWindow::update();
}

void WarfareWindow::wheelEvent (QWheelEvent* event) {
  hexDrawer->zoom(event->delta());
  update(); 
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
      if (Hex::None == clickedHex->getDirection(clickedLine)) {
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
      if (8 < clickedHex->getDevastation()) {
	Logger::logStream(Logger::Warning) << "Hex is too devastated.\n"; 
	return;
      }
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
    if (selectedHex) selDrawer->setSelected(selectedHex);
    if (selectedVertex) selDrawer->setSelected(selectedVertex);
    if (selectedLine) selDrawer->setSelected(selectedLine);
    update();
    return; 
  }

  Action act;
  act.player = currentPlayer;
  
  if (clickedHex) {
    if (selectedVertex) {
      // Vertex to Hex: Devastate
      if (Hex::NoVertex == clickedHex->getDirection(selectedVertex)) {
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
      if (selectedVertex->getUnit(0)->weakened()) {
	Logger::logStream(Logger::Warning) << "Weakened unit cannot raid.\n";
	return; 
      }
      act.todo = Action::Devastate;
      act.start = selectedVertex;
      act.target = clickedHex; 
    }
    else if ((selectedHex) && (selectedHex == clickedHex)) {
      if (0 >= clickedHex->getDevastation()) {
	Logger::logStream(Logger::Warning) << "Hex is not damaged, cannot be repaired.\n"; 
	return;
      }
      act.todo = Action::Repair;
      act.target = clickedHex;
      act.source = clickedHex; 
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
      if (Hex::NoVertex == selectedVertex->getDirection(clickedVertex)) {
	Logger::logStream(Logger::Warning) << "Not adjacent.\n";
	return; 
      }
      if ((0 < clickedVertex->numUnits()) && (clickedVertex->getUnit(0)->getOwner() == currentPlayer)) {
      	Logger::logStream(Logger::Warning) << "Friendly unit is in the way.\n"; 
	return;
      }
      if ((selectedVertex->getUnit(0)->weakened()) && (0 < clickedVertex->numUnits())) {
	Logger::logStream(Logger::Warning) << "Weakened unit cannot attack.\n";
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
      if (!selectedVertex->getUnit(0)->weakened()) {
	Logger::logStream(Logger::Warning) << "Not weakened, cannot be reinforced.\n"; 
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
	if (Castle::maxGarrison <= clickedLine->getCastle()->numGarrison()) {
	  Logger::logStream(Logger::Warning) << "Castle is full.\n";
	  return;
	}
	
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
      if (Castle::maxGarrison <= clickedLine->getCastle()->numGarrison()) {
	Logger::logStream(Logger::Warning) << "Castle is fully garrisoned, cannot reinforce.\n"; 
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
  std::set<Player*> stillHaveCastles;
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
  Action::ActionResult res = act.execute(currentGame);
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
    hexDrawer->draw(0, 0, 0); 
    return; 
  }
  if (turnEnded()) endOfTurn(); 
  
  currentPlayer = Player::nextPlayer(currentPlayer);
  runNonHumans();
  if (selectedHex) selDrawer->setSelected(selectedHex);
  if (selectedVertex) selDrawer->setSelected(selectedVertex);
  if (selectedLine) selDrawer->setSelected(selectedLine);
}

void WarfareWindow::runNonHumans () {
  while (!currentPlayer->isHuman()) {
    Logger::logStream(Logger::Debug) << currentPlayer->getDisplayName() << "\n";
    currentPlayer->getAction(currentGame);
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
  selDrawer->setFixedSize(220, 100);
  for (Player::Iterator p = Player::begin(); p != Player::end(); ++p) {
    hexDrawer->assignColour(*p); 
  }
  hexDrawer->loadSprites(); 
}

void WarfareWindow::clearGame () {
    if (currentGame) {
    hexDrawer->clearColours();
    delete currentGame;
    currentGame = 0;
    currentPlayer = 0;
    hexDrawer->draw(0, 0, 0); 
  }
}
