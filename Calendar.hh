#ifndef CALENDAR_HH
#define CALENDAR_HH
#include <string> 

namespace Calendar {

  enum Season {Winter, Spring, Summer, Autumn}; 
  Season getCurrentSeason (); 
  void newWeekBegins (); 
  void newYearBegins (); 

  void setWeek (int w);
  int turnsToAutumn ();
  std::string toString ();
  int currentWeek (); 
};



#endif
