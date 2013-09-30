#ifndef DIRECTION_HH
#define DIRECTION_HH

#include <string>
using namespace std; 

enum Direction {NorthWest = 0, North, NorthEast, SouthEast, South, SouthWest, NoDirection};
enum Vertices {LeftUp = 0, RightUp, Right, RightDown, LeftDown, Left, NoVertex}; 


Direction operator+ (Direction a, int b);
Direction operator- (Direction a, int b);
Vertices operator+ (Vertices a, int b);
Vertices operator- (Vertices a, int b);



string getDirectionName (Direction dat);
string getVertexName (Vertices dat);  
Direction getDirection (string n);  
Vertices getVertex (string n);
Direction convertToDirection (int n);
Vertices convertToVertex (int i);
Direction oppositeDirection (Direction dat);
Vertices oppositeVertex (Vertices dat);

#endif 
