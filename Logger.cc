#include "Logger.hh"
#include <cassert> 

std::map<int, Logger*> Logger::logs; 

Logger::Logger () :
  active(true),
  buffer()
{}

Logger::~Logger () {}

Logger& Logger::operator<< (std::string dat) {
  if (!active) return *this;
  std::size_t linebreak = dat.find_first_of('\n');
  if (std::string::npos == linebreak) buffer.append(dat.c_str());
  else {
    buffer.append(dat.substr(0, linebreak).c_str());
    clearBuffer();
    (*this) << dat.substr(linebreak+1); 
  }
  return *this; 
}

Logger& Logger::operator<< (char* dat) {
  if (!active) return *this;
  return ((*this) << std::string(dat)); 
}

Logger& Logger::operator<< (const char* dat) {
  if (!active) return *this;
  return ((*this) << std::string(dat)); 
}

Logger& Logger::operator<< (QString dat) {
  if (!active) return *this;
  int linebreak = dat.indexOf('\n');
  if (-1 == linebreak) buffer.append(dat);
  else {
    buffer.append(dat.mid(0, linebreak));
    clearBuffer();
    (*this) << dat.mid(linebreak+1); 
  }
  
  return *this;  
}

Logger& Logger::operator<< (int dat) {
  if (!active) return *this;
  buffer.append(QString("%1").arg(dat)); 
  return *this; 
}

Logger& Logger::operator<< (double dat) {
  if (!active) return *this;
  buffer.append(QString("%1").arg(dat)); 
  return *this; 
}

Logger& Logger::operator<< (char dat) {
  if (!active) return *this;
  if ('\n' == dat) clearBuffer();
  else buffer.append(dat);
  return *this; 
}

void Logger::createStream (int idx) {
  logs[idx] = new Logger(); 
}

Logger& Logger::logStream (int idx) {
  assert(logs[idx]); 
  return *(logs[idx]); 
}

void Logger::clearBuffer () {
  if (!active) return; 
  emit message(buffer); 
  buffer.clear();
}
