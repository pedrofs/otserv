//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////


#ifndef __monster_h_
#define __monster_h_


#include "creature.h"
#include "game.h"
#include "tile.h"
#include "templates.h"

class Creature;

class TimeProbabilityClass {
public:
	TimeProbabilityClass()
	{
		setDefault();
	}

	TimeProbabilityClass(int _cycleTicks, int _probability, int _exhaustionticks)
	{
		setDefault();
		init(_cycleTicks, _probability, _exhaustionticks);
	};

	~TimeProbabilityClass() {};

	bool onTick(int ticks)
	{
		ticksleft -= ticks;
		
		if(ticksleft <= 0) {
			ticksleft = cycleTicks;
			bool ret = (random_range(1, 100) <= probability);
			return ret;
		}

		return false;
	}
	
	void init(int _cycleTicks, int _probability, int _exhaustionticks)
	{
		if(_cycleTicks >= 0) {
			this->ticksleft = _cycleTicks;
			this->cycleTicks = _cycleTicks;
		}

		if(_probability >= 0)
			probability = std::min(100, _probability);

		if(_exhaustionticks >= 0)
			exhaustionTicks = _exhaustionticks;
	}
  	
	int getExhaustion() const {return exhaustionTicks;}

private:
	void setDefault()
	{
		cycleTicks = 2000;
		ticksleft = cycleTicks;
		probability = 80;
		exhaustionTicks = 0;
	}

	int ticksleft;
	int cycleTicks;
	int probability;
	int exhaustionTicks;
};

class PhysicalAttackClass {
public:
	PhysicalAttackClass()
	{
		//fighttype = FIGHT_NONE;
		disttype = DIST_NONE;
		minWeapondamage = 0;
		maxWeapondamage = 1;
	};

	~PhysicalAttackClass() {};

	fight_t fighttype;
	subfight_t disttype;

	int minWeapondamage;
	int maxWeapondamage;
};

enum monsterstate_t {
	STATE_IDLE,
	STATE_TARGETNOTREACHABLE,
	STATE_ATTACKING,
	STATE_FLEEING
};

class Monster : public Creature
{
public:
	Monster(const std::string& name, Game* game);
  virtual ~Monster();
	//const Monster& operator=(const Monster& rhs);
	//virtual unsigned long idRange(){ return 0x40000000;}
	// use range 0x10000000 to able use runes on monsters
	virtual unsigned long idRange(){ return 0x10000000;}
	static AutoList<Monster> listMonster;
	void removeList() {listMonster.removeList(getID());}
	void addList() {listMonster.addList(this);}
	
	virtual void useThing() {
		//std::cout << "Monster: useThing() " << this << std::endl;
		useCount++;
	};
	
	virtual void releaseThing() {
		//std::cout << "Monster: releaseThing() " << this << std::endl;
		useCount--;
		if (useCount == 0)
			delete this;
	};

	virtual int getArmor() const {
  		return this->armor;
  	}
  	
	virtual int getDefense() const {
  		return this->defense;
  	}
	
	virtual void onAttack();
	bool isLoaded() const {return loaded;}

private:
	Game* game;
	std::list<Position> route;
	monsterstate_t state;
	bool updateRoute;
	int oldThinkTicks;
	Position targetPos;
	Position moveToPos;
	int armor;
	int defense;
	void doMoveTo(int dx, int dy);
	bool hasDistanceAttack;
	bool canPushItems;
	unsigned long staticLook;
	unsigned long staticAttack;
	unsigned short changeTargetChance;

	int getCurrentDistanceToTarget();
	int getTargetDistance();
	void calcMovePosition();
	void randMovePosition();
	void updateLookDirection();
	void getCloseCombatPosition(const Position &target, Position &dest);
	bool canMoveTo(unsigned short x, unsigned short y, unsigned char z);
	bool isInRange(const Position &pos);
	bool isCreatureReachable(const  Creature* creature);
	Creature* findTarget(long range, bool &canReach, const Creature *ignoreCreature = NULL);
	void stopAttack();
	void stopThink();
	void getSleepTicks(long long &delay, int& stepDuration);

	#define CHANCE_MAX  100000
	bool LoadLootNode(xmlNodePtr);
	bool LoadLootContainer(xmlNodePtr,Container*);	
	Item* LoadLootItemStackable(xmlNodePtr,unsigned short);
	Item* LoadLootItem(xmlNodePtr,unsigned short);
	unsigned long GetRandom();


protected:
	int useCount;
	PhysicalAttackClass	*curPhysicalAttack;

	int targetDistance;
	int runAwayHealth;
	bool pushable;

	bool doAttacks(Player* attackedPlayer, bool aggressive = false);

	typedef std::vector<TimeProbabilityClass> TimeProbabilityClassVec;

	typedef std::map<std::string, TimeProbabilityClassVec> InstantAttackSpells;
	InstantAttackSpells instantSpells;
	
	typedef std::map<unsigned short, TimeProbabilityClassVec> RuneAttackSpells;
	RuneAttackSpells runeSpells;

	typedef std::map<PhysicalAttackClass*, TimeProbabilityClass> PhysicalAttacks;
	PhysicalAttacks physicalAttacks;

	typedef std::vector<std::pair<std::string, TimeProbabilityClass> > YellingSentences;
	YellingSentences yellingSentences;

	std::vector<Item *> lootItems;

	virtual fight_t getFightType() {return curPhysicalAttack->fighttype;};
	virtual subfight_t getSubFightType()  {return curPhysicalAttack->disttype;}
	virtual int getWeaponDamage() const;

	void OnCreatureEnter(const Creature *creature, bool canReach = true);
	void OnCreatureLeave(const Creature *creature);
	bool validateDistanceAttack(const Creature *creature);
	bool validateDistanceAttack(const Position &pos);
	bool monsterMoveItem(Item* item, int radius);

	virtual int getLostExperience() {return experience;};
	virtual void dropLoot(Container *corpse);

	virtual void onThingMove(const Creature *creature, const Thing *thing, const Position *oldPos,
		unsigned char oldstackpos, unsigned char oldcount, unsigned char count);

  virtual void onCreatureAppear(const Creature *creature);
  virtual void onCreatureDisappear(const Creature *creature, unsigned char stackPos, bool tele);
  virtual void onThingDisappear(const Thing* thing, unsigned char stackPos);
  virtual void onThingTransform(const Thing* thing,int stackpos);
  virtual void onThingAppear(const Thing* thing);
  virtual void onTeleport(const Creature *creature, const Position *oldPos, unsigned char oldstackpos);

	virtual bool isAttackable() const { return true; };
  virtual bool isPushable() const { return pushable; };

	virtual int onThink(int& newThinkTicks);
  virtual void setAttackedCreature(unsigned long id);
  //virtual std::string getDescription() const;
	std::string getDescription(bool self) const;
	bool loaded;
};

#endif // __monster_h_
