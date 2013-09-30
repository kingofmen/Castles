#include "Directions.hh"


Direction operator+ (Direction a, int b) {
  int temp = a;
  temp += b; 
  return convertToDirection(temp); 
}

Direction operator- (Direction a, int b) {
  int temp = a;
  temp -= b; 
  return convertToDirection(temp); 
}

Vertices operator+ (Vertices a, int b) {
  int temp = a;
  temp += b; 
  return convertToVertex(temp); 
}

Vertices operator- (Vertices a, int b) {
  int temp = a;
  temp -= b;   
  return convertToVertex(temp); 
}

Direction getDirection (string n) {
  if (n == "North") return North;
  if (n == "South") return South;
  if (n == "NorthEast") return NorthEast;
  if (n == "NorthWest") return NorthWest;
  if (n == "SouthEast") return SouthEast;
  if (n == "SouthWest") return SouthWest;
  return NoDirection; 
}

Vertices getVertex (string n) {
  if (n == "UpLeft") return LeftUp;
  if (n == "UpRight") return RightUp;
  if (n == "Left") return Left;
  if (n == "Right") return Right;
  if (n == "DownLeft") return LeftDown;
  if (n == "DownRight") return RightDown; 
  return NoVertex; 
}

string getDirectionName (Direction dat) {
  switch (dat) {
  case NorthWest: return string("NorthWest");
  case NorthEast: return string("NorthEast");
  case North: return string("North");
  case South: return string("South");
  case SouthWest: return string("SouthWest");
  case SouthEast: return string("SouthEast");
  case NoDirection:
  default:
    return string("None");
  }
}

string getVertexName (Vertices dat) {
  switch (dat) {
  case LeftUp: return string("UpLeft");
  case RightUp: return string("UpRight");
  case Left: return string("Left");
  case Right: return string("Right");
  case LeftDown: return string("DownLeft");
  case RightDown: return string("DownRight");
  case NoDirection:
  default:
    return string("None");
  }
}

Direction convertToDirection (int n){
  if (n >= 0) n %= NoDirection;
  else {
    const static int temp = NoDirection;
    n = temp - ((-n) % NoDirection);
  }
  
  switch (n) {
  case NorthWest: return NorthWest;
  case NorthEast: return NorthEast;
  case SouthWest: return SouthWest;
  case SouthEast: return SouthEast;
  case North: return North;
  case South: return South; 
  default: return NoDirection; 
  }
}

Vertices convertToVertex (int i) {
  if (i >= 0) i %= NoVertex;
  else {
    const static int temp = NoVertex;
    i = temp - ((-i) % NoVertex);
  }
  
  switch (i) {
  case LeftUp: return LeftUp;
  case RightUp: return RightUp;
  case Right: return Right;
  case RightDown: return RightDown;
  case LeftDown: return LeftDown;
  case Left: return Left;
  default:
  case NoVertex: return NoVertex;
  }
  return NoVertex; 
}

Direction oppositeDirection (Direction dat) {
  switch (dat) {
  case NorthWest: return SouthEast;
  case NorthEast: return SouthWest;
  case North: return South;
  case South: return North;
  case SouthEast: return NorthWest;
  case SouthWest: return NorthEast; 
  case NoDirection: return NoDirection; 
  default: return NoDirection; 
  }
  return NoDirection; 
}

Vertices oppositeVertex (Vertices dat) {
  switch (dat) { 
  case LeftUp:    return RightDown;
  case RightUp:   return LeftDown;
  case Left:      return Right;
  case Right:     return Left;
  case RightDown: return LeftUp;
  case LeftDown:  return RightUp; 
  case NoVertex: return NoVertex;
  default: return NoVertex;
  }
  return NoVertex; 
}
