#ifndef PLAYER_GRAPHICS_HH
#define PLAYER_GRAPHICS_HH

#include <QtOpenGL>

class PlayerGraphicsInfo {
  friend class StaticInitialiser;
public:
  PlayerGraphicsInfo ();
  ~PlayerGraphicsInfo ();

  int getRed () const {return qRed(colour);}
  int getGreen () const {return qGreen(colour);}
  int getBlue () const {return qBlue(colour);}
  GLuint getFlagTexture () const {return flag_texture_id;}

private:
  QRgb colour;
  GLuint flag_texture_id;
};

#endif
