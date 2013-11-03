#ifndef LOGGER_HH
#define LOGGER_HH

#include <QObject> 
#include <string> 
#include <map> 
#include "Parser.hh"
#include <fstream>
#include <ios> 

enum Debugs {DebugGeneral = 30,
	     DebugAI = 30,
	     DebugBuildings,
	     DebugProduction,
	     DebugTrade, 
	     NumDebugs};

const int DebugStartup = NumDebugs+1; 

class Logger : public QObject {
  Q_OBJECT 
public: 
  Logger (); 
  ~Logger (); 
  enum DefaultLogs {Debug = 0, Trace, Game, Warning, Error}; 

  Logger& append (unsigned int prec, double val); 
  
  Logger& operator<< (std::string dat);
  Logger& operator<< (QString dat);
  Logger& operator<< (int dat);
  Logger& operator<< (unsigned int dat);
  Logger& operator<< (double dat);
  Logger& operator<< (char dat);
  Logger& operator<< (char* dat);
  Logger& operator<< (const char* dat);
  Logger& operator<< (Object* dat);
  Logger& operator<< (void* dat); 
  void setActive (bool a) {active = a;}
  void setPrecision (int p = -1) {precision = p;}
  bool isActive () const {return active;} 
  
  static void createStream (int idx); 
  static Logger& logStream (int idx); 

signals:
  void message (QString m);
  
private:
  void clearBuffer (); 
  
  bool active;
  QString buffer; 
  int precision;
  
  static std::map<int, Logger*> logs; 
};

class FileLog : public QObject {
  // Helper class for copying a log stream to a file.
  // Intended for debugging.
  Q_OBJECT

public:
  FileLog (std::string fname);
  ~FileLog ();

public slots:
  void message (QString str);

private:
  std::ofstream writer;
  std::string filename; 
};

#endif
 
