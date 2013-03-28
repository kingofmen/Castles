#include "UtilityFunctions.hh"
//#include <vector>
#include <cmath>
#include <algorithm> 

char strbuffer[1000]; 

double degToRad (double degrees) {
  return degrees * 3.14159265 / 180; 
}
double radToDeg (double radians) {
  return radians * 180 / 3.14159265; 
}

triplet triplet::cross (const triplet& other) const {
  triplet ret;
  ret.x() = this->y() * other.z() - this->z() * other.y();
  ret.y() = this->z() * other.x() - this->x() * other.z();
  ret.z() = this->x() * other.y() - this->y() * other.x(); 
  return ret;
}

double triplet::dot (const triplet& other) const {
  double ret = this->x() * other.x();
  ret       += this->y() * other.y();
  ret       += this->z() * other.z();
  return ret; 
}

double triplet::angle (const triplet& other) const {
  double ret = this->dot(other);
  ret /= this->norm();
  ret /= other.norm();
  return acos(ret); 
}

void triplet::normalise () {
  (*this) /= this->norm();
}



void triplet::operator-= (const triplet& other) {
  this->x() -= other.x();
  this->y() -= other.y();
  this->z() -= other.z();  
}
void triplet::operator+= (const triplet& other) {
  this->x() += other.x();
  this->y() += other.y();
  this->z() += other.z();  
}
void triplet::operator/= (double scale) {
  this->x() /= scale;
  this->y() /= scale;
  this->z() /= scale;
}
void triplet::operator*= (double scale) {
  this->x() *= scale;
  this->y() *= scale;
  this->z() *= scale;
}

int convertFractionToInt (double fraction) {
  // Returns the integer part of fraction,
  // plus 1 with probability equal to the
  // fractional part. 
  
  int ret = (int) floor(fraction);
  fraction -= ret;
  double roll = rand();
  roll /= RAND_MAX;
  if (roll < fraction) ret++;
  return ret; 
}

bool intersect (double line1x1, double line1y1, double line1x2, double line1y2,
		double line2x1, double line2y1, double line2x2, double line2y2) {

  double line1A = line1y2 - line1y1;
  double line1B = line1x1 - line1x2; 
  double line1C = line1A*line1x1 + line1B*line1y1;

  double line2A = line2y2 - line2y1;
  double line2B = line2x1 - line2x2; 
  double line2C = line2A*line2x1 + line2B*line2y1;

  double det = line1A*line2B - line1B*line2A;
  if (fabs(det) < 0.00001) return false; // Parallel lines

  double xinter = (line2B*line1C - line1B*line2C)/det;
  static const double tol = 0.0001;
  if (xinter < std::min(line1x1, line1x2) - tol) return false;  // Small tolerance accounts for roundoff error
  if (xinter > std::max(line1x1, line1x2) + tol) return false;
  if (xinter < std::min(line2x1, line2x2) - tol) return false;
  if (xinter > std::max(line2x1, line2x2) + tol) return false;
  double yinter = (line1A*line2C - line2A*line1C)/det;
  if (yinter < std::min(line1y1, line1y2) - tol) return false;
  if (yinter > std::max(line1y1, line1y2) + tol) return false;
  if (yinter < std::min(line2y1, line2y2) - tol) return false;
  if (yinter > std::max(line2y1, line2y2) + tol) return false;

  return true;
}

string remQuotes (string tag) {
  string ret;
  bool found = false; 
  for (unsigned int i = 0; i < tag.size(); ++i) {
    char curr = tag[i];
    if (!found) {
      if (curr != '"') continue;
      found = true;
      continue;
    }

    if (curr == '"') break;
    ret += curr; 
  }
  return ret; 
}

string outcomeToString (Outcome out) {
  switch (out) {
  case Disaster:   return "Disaster";
  case Bad:        return "Bad";
  case Neutral:    return "Neutral";
  case Good:       return "Good";
  case VictoGlory: return "VictoGlory";
  case NumOutcomes:
  default:
    return "Unknown outcome type"; 
  }

  return "This cannot happen"; 
}

