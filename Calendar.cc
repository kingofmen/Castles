#include "Calendar.hh"
#include <cstdio> 


const int seasonLength = 14;
int week = 0; 

namespace Calendar {

  void newWeekBegins () {week++;}
  void newYearBegins () {week = 0;}
  void setWeek (int w) {week = w;}
  int currentWeek () {return week;} 
  
  Season getCurrentSeason () {
    if (1*seasonLength > week) return Spring;
    if (2*seasonLength > week) return Summer;
    if (3*seasonLength > week) return Autumn;
    return Winter;
  }

  int turnsToNextSeason () {
    // Includes this turn, hence +1. 
    return 1 + seasonLength - (week % seasonLength);
  }
  
  int turnsToAutumn () {
    int remainsOfSeason = seasonLength - (week % seasonLength);
    int otherSeasons = 0; 
    switch (getCurrentSeason()) {
    default: 
    case Spring: otherSeasons = 1; break;
    case Summer: otherSeasons = 0; break;
    case Winter: otherSeasons = 2; remainsOfSeason = 0; break;
    case Autumn: otherSeasons = 2; break; // Winter is not a full-length season. 
    }
    return remainsOfSeason + otherSeasons*seasonLength; 
  }

  std::string toString () {
    std::string ret; 
    switch (getCurrentSeason()) {
    default:
    case Spring: ret = "Spring"; break;
    case Summer: ret = "Summer"; break;
    case Winter: ret = "Winter"; break;
    case Autumn: ret = "Autumn"; break;
    }
    static char buf[1000];
    sprintf(buf, "%s %i", ret.c_str(), week % seasonLength);
    ret = buf;
    return ret; 
  }
  
};
