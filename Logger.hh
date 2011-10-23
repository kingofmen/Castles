#ifndef LOGGER_HH
#define LOGGER_HH

#include <QObject> 
#include <string> 
#include <map> 

class Logger : public QObject {
  Q_OBJECT 
public: 
  Logger (); 
  ~Logger (); 
  enum DefaultLogs {Debug = 0, Trace, Game, Warning, Error}; 
  
  Logger& operator<< (std::string dat);
  Logger& operator<< (QString dat);
  Logger& operator<< (int dat);
  Logger& operator<< (double dat);
  Logger& operator<< (char dat);
  Logger& operator<< (char* dat);
  Logger& operator<< (const char* dat);  
  void setActive (bool a) {active = a;}

  static void createStream (int idx); 
  static Logger& logStream (int idx); 

signals:
  void message (QString m);
  
private:
  void clearBuffer (); 
  
  bool active;
  QString buffer; 
  
  static std::map<int, Logger*> logs; 
};


#endif
