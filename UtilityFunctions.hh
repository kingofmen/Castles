#ifndef UTILITIES_HH
#define UTILITIES_HH

#include <cstdlib> 
#include <map>
#include <cmath> 
#include <string>
#include <vector>
#include "boost/tuple/tuple.hpp"
#include <QtOpenGL>

using namespace std;
enum Outcome {Disaster = 0, Bad, Neutral, Good, VictoGlory, NumOutcomes}; 
extern char strbuffer[1000]; 

double degToRad (double degrees);
double radToDeg (double radians); 
GLuint loadTexture (string fname, QColor backup, GLuint index = 0);  // Implemented in StaticInitialiser. 

enum RollType {Equal = 0, GtEqual, LtEqual, Greater, Less};

struct DieRoll {
  DieRoll (int d, int f);   
  double probability (int target, int mods, RollType t) const; 
  int roll () const;
  
private:
  double baseProb (int target, int mods, RollType t) const; 

  int dice;
  int faces;
  DieRoll* next; 
};

class Named {
public:
  string getName() const {return name;}
  void setName (string n) {name = n;} 
private:
  string name;
};

string outcomeToString (Outcome out);

struct doublet : public boost::tuple<double, double> {
  // Adding a touch of syntactic sugar to the tuple. 
  
  doublet (double x, double y) : boost::tuple<double, double>(x, y) {}
  doublet () : boost::tuple<double, double>(0, 0) {}

  double& x () {return boost::get<0>(*this);}
  double& y () {return boost::get<1>(*this);}

  double x () const {return boost::get<0>(*this);}
  double y () const {return boost::get<1>(*this);}

  double  angle (const doublet& other) const;  
  double  dot   (const doublet& other) const;
  double  norm  () const {return sqrt(pow(this->x(), 2) + pow(this->y(), 2));}
  void    normalise ();
  void    rotate (double degrees, const doublet& around = zero);

  void operator/= (double scale);
  void operator*= (double scale);  
  void operator-= (const doublet& other);
  void operator+= (const doublet& other);
  //void operator+= (const triplet& other);  

private:
  static const doublet zero; 
}; 

struct triplet : public boost::tuple<double, double, double> {
  // Adding a touch of syntactic sugar to the tuple. 
  
  triplet (double x, double y, double z) : boost::tuple<double, double, double>(x, y, z) {}
  triplet (double x, double y)           : boost::tuple<double, double, double>(x, y, 0) {}
  triplet (double x)                     : boost::tuple<double, double, double>(x, 0, 0) {}    
  triplet ()                             : boost::tuple<double, double, double>(0, 0, 0) {}

  double& x () {return boost::get<0>(*this);}
  double& y () {return boost::get<1>(*this);}
  double& z () {return boost::get<2>(*this);}

  double x () const {return boost::get<0>(*this);}
  double y () const {return boost::get<1>(*this);}
  double z () const {return boost::get<2>(*this);}

  double  angle (const triplet& other) const;  
  triplet cross (const triplet& other) const;
  double  dot   (const triplet& other) const;
  double  norm  () const {return sqrt(pow(this->x(), 2) + pow(this->y(), 2) + pow(this->z(), 2));}
  void    normalise (); 

  void operator/= (double scale);
  void operator*= (double scale);  
  void operator-= (const triplet& other);
  void operator+= (const triplet& other);

  void rotatexy (double degrees, const triplet& around = zero); // Special for rotating the xy projection.  

private:
  static const triplet zero; 
};

triplet operator- (triplet one, triplet two);
triplet operator+ (triplet one, triplet two);
triplet operator* (triplet one, double scale);
triplet operator/ (triplet one, double scale);

int convertFractionToInt (double fraction);
bool intersect (double line1x1, double line1y1, double line1x2, double line1y2,
		double line2x1, double line2y1, double line2x2, double line2y2); 
string remQuotes (string tag); 
bool contains (vector<triplet> const& polygon, triplet const& point); 

template <class T>
T getKeyByWeight (const map<T, int>& mymap) {
  // What if some of the weights are negative? 
  
  if (mymap.empty()) return (*(mymap.begin())).first; 
  
  int totalWeight = 0;
  for (typename map<T, int>::const_iterator i = mymap.begin(); i != mymap.end(); ++i) {
    totalWeight += (*i).second;
  }
  if (0 == totalWeight) {
    int roll = rand() % mymap.size();
    typename map<T, int>::const_iterator i = mymap.begin();
    for (int j = 0; j < roll; ++j) ++i;
    return (*i).first;
  }

  int roll = rand() % totalWeight;
  int counter = 0; 
  for (typename map<T, int>::const_iterator i = mymap.begin(); i != mymap.end(); ++i) {
    if (0 == (*i).second) continue;
    if (counter >= roll) return (*i).first;
    counter += (*i).second;
  }
  return (*(mymap.begin())).first;
}

class MilUnitTemplate; 
class MilStrength {
 public:
  virtual int getUnitTypeAmount (MilUnitTemplate const* const ut) const = 0;
  int getTotalStrength () const;

  static double greatestStrength; 
};

#endif 

