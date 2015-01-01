#ifndef CALENDAR_HH
#define CALENDAR_HH
#include <string> 

namespace Calendar {
  extern const double inverseYearLength;
  enum Season {Winter, Spring, Summer, Autumn}; 
  Season getCurrentSeason (); 
  void newWeekBegins (); 
  void newYearBegins (); 

  void setWeek (int w);
  int turnsToAutumn ();
  int turnsToNextSeason (); 
  std::string toString ();
  int currentWeek (); 
};



#endif
