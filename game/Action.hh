#ifndef ACTION_HH
#define ACTION_HH

#include <cstdlib> 
#include <string> 
#include "Hex.hh" 
#include "UtilityFunctions.hh" 

class Player; 
class Line; 
class Vertex;
class MilUnit;
class MilUnitTemplate; 
class Castle; 

extern const DieRoll FiveDSix; 
extern const DieRoll FourDSix;
extern const DieRoll ThreeDSix; 
extern const DieRoll TwoDSix;
extern const DieRoll OneDSix;
extern const DieRoll OneDHun; 

struct Action {
  friend class StaticInitialiser; 
public:
  enum ActionResult {Ok = 0, Fail, NotAdjacent, NotEnoughPops, AttackFails, WrongPlayer, Impassable, NoBuilding, NotImplemented}; 
  //enum Outcome {Disaster = 0, NoChange, Success, NumOutcomes};
  
  Action ();
  ActionResult execute (); 
  ActionResult checkPossible () {return (this->*(todo.check))();}
  double probability (Outcome dis);
  void undo (); 
  void makeHypothetical ();
  void forceOutcome (Outcome f) {force = f;} 
  std::string describe () const;
  
  Player* player;
  int numUnits;
  Hex* source;
  Hex* target;
  Vertex* start;
  Vertex* final;
  Line* begin;
  Line* cease;
  MilUnit* temporaryUnit;
  const MilUnitTemplate* unitType; 
  bool print;
  Outcome force;
  Outcome result; 

  Vertices activeRear;
  Vertices forcedRear; 
  
private:
  struct Calculator {
    Calculator (); 
    const DieRoll* die;
    int* results;
  };
  struct ThingsToDo {
    ThingsToDo (Calculator* p, ActionResult (Action::*e) (Outcome), ActionResult (Action::*c) (), std::string n);
    Calculator* calc; 
    ActionResult (Action::*exec) (Outcome out);
    ActionResult (Action::*check) ();
    std::string name;

    bool operator== (const ThingsToDo& dat) {return name == dat.name;} 
  };

  static Calculator* attackCalculator;
  static Calculator* mobiliseCalculator;
  static Calculator* devastateCalculator;
  static Calculator* surrenderCalculator;
  static Calculator* recruitCalculator;
  static Calculator* colonyCalculator;
  static Calculator* successCalculator;   
  
  ActionResult alwaysPossible () {return Ok;} 
  ActionResult canAttack ();
  ActionResult canMobilise ();
  ActionResult canBuild ();
  ActionResult canEnter (); 
  ActionResult canDevastate ();
  ActionResult canReinforce ();
  ActionResult canSurrender ();
  ActionResult canRecruit ();
  //ActionResult canRepair ();

  ActionResult noop (Outcome out); 
  ActionResult attack (Outcome out);
  ActionResult mobilise (Outcome out);
  ActionResult garrison (Outcome out);
  ActionResult build (Outcome out);
  ActionResult devastate (Outcome out);
  ActionResult reinforce (Outcome out);
  ActionResult surrender (Outcome out);
  ActionResult recruit (Outcome out);
  //ActionResult repair (Outcome out); 
  
public:
  ThingsToDo todo;
  static const ThingsToDo EndTurn;
  static const ThingsToDo Colonise;
  static const ThingsToDo Attack;
  static const ThingsToDo Mobilise;
  static const ThingsToDo BuildFortress;
  static const ThingsToDo Devastate;
  static const ThingsToDo Reinforce;
  static const ThingsToDo CallForSurrender;
  static const ThingsToDo Recruit;
  //static const ThingsToDo Repair;
  static const ThingsToDo Nothing; 
  static const ThingsToDo EnterGarrison; 
};

#endif
