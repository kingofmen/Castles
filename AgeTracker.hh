#ifndef AGETRACKER_HH
#define AGETRACKER_HH

#include "Mirrorable.hh" 
#include <vector>
#include <cassert> 
using namespace std; 

class AgeTracker : public Mirrorable<AgeTracker> {
  friend class Mirrorable<AgeTracker>;
  friend class StaticInitialiser; 
public:
  AgeTracker ();
  AgeTracker (AgeTracker* other);
  ~AgeTracker () {} 

  void addPop (int number, int age);
  void addPop (AgeTracker& other); 
  void age ();
  void clear (); 
  void die (int number);
  void dieExactly (int number);
  int getTotalPopulation () const;
  int getPop (int age) const {return people[age];} 
  virtual void setMirrorState ();

  static const int maxAge;
  
private: 
  vector<int> people;

  
}; 

#endif 
