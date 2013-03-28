#include "AgeTracker.hh"
#include "UtilityFunctions.hh"

const int AgeTracker::maxAge = 80;

AgeTracker::AgeTracker ()
  : Mirrorable<AgeTracker>()
  , people(maxAge)
{}

AgeTracker::AgeTracker (AgeTracker* other)
  : Mirrorable<AgeTracker>(other)
  , people(maxAge)
{}


void AgeTracker::addPop (int number, int age) {
  assert(age < maxAge);
  assert(age >= 0);
  people[age] += number;
  if (people[age] < 0) people[age] = 0;
}

void AgeTracker::addPop (AgeTracker& other) {
  for (int age = 0; age < maxAge; ++age) {
    people[age] += other.people[age]; 
  }
}

void AgeTracker::age () {
  for (int i = maxAge-1; i > 0; --i) {
    people[i] = people[i-1]; 
  }
  people[0] = 0; 
}

void AgeTracker::clear () {
  people.clear();
  people.resize(maxAge);
}

void AgeTracker::die (int number) {
  if (number <= 0) return;
  if (number >= getTotalPopulation()) {
    clear();
    return;
  }

  double total = getTotalPopulation();
  for (int i = 0; i < maxAge; ++i) {
    if (0 == people[i]) continue;
    double fraction = people[i] / total;
    int loss = convertFractionToInt(number * fraction);
    addPop(-loss, i);
  }
}

int AgeTracker::getTotalPopulation () const {
  int ret = 0;
  for (int i = 0; i < maxAge; ++i) {
    ret += people[i];
  }
  return ret; 
}

void AgeTracker::setMirrorState () {
  for (int i = 0; i < maxAge; ++i) {
    mirror->people[i] = people[i]; 
  }
}
