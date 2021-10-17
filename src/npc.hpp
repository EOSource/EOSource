#ifndef NPC_HPP_INCLUDED
#define NPC_HPP_INCLUDED

#include "fwd/npc.hpp"

#include <list>
#include <string>
#include <array>
#include <unordered_map>
#include <vector>

#include "fwd/character.hpp"
#include "fwd/eodata.hpp"
#include "fwd/map.hpp"

/**
 * Used by the NPC class to store information about an attacker
 */
struct NPC_Opponent
{
	Character *attacker;
	int damage;
	double last_hit;
};

/**
 * Used by the NPC class to store information about an item drop
 */
struct NPC_Drop
{
	unsigned short id;
	int min;
	int max;
	double chance;
};

/**
 * Used by the NPC class to store trade shop data
 */
struct NPC_Shop_Trade_Item
{
	unsigned short id;
	int buy;
	int sell;
};

/**
 * Used by the NPC_Shop_Craft_Item class to store item ingredients
 */
struct NPC_Shop_Craft_Ingredient
{
	unsigned short id;
	unsigned char amount;
};

/**
 * Used by the NPC class to store craft shop data
 */
struct NPC_Shop_Craft_Item
{
	unsigned short id;
	std::vector<NPC_Shop_Craft_Ingredient *> ingredients;
};

/**
 * Used by the NPC class to store innkeeper citizenship information
 */
struct NPC_Citizenship
{
	std::string home;
	std::array<std::string, 3> questions;
	std::array<std::string, 3> answers;
};

/**
 * Used by the NPC class to store wedding information
 */
struct NPC_Marriage
{
    Character *partner[2];
    bool partner_accepted[2];
    bool request_accepted;
    unsigned char state;
    double last_execution;

    NPC_Marriage() : request_accepted(false), state(1), last_execution(0.0)
    {
        for (int i = 0; i < 2; ++i) partner_accepted[i] = false;
    }
};

/**
 * Used by the NPC class to store skill master data
 */
struct NPC_Learn_Skill
{
	unsigned short id;
	int cost;
    unsigned char levelreq;
    unsigned char classreq;
	std::array<short, 4> skillreq;
	short strreq, intreq, wisreq, agireq, conreq, chareq;
};

/**
 * An instance of an NPC created and managed by a Map
 */
class NPC
{
    public:

        Character *owner;

		short id;
		short spawn_time;

		unsigned char x, y;

		Direction direction;

		unsigned char spawn_type;
		unsigned char spawn_x, spawn_y;
		unsigned char index;

		static void SetSpeedTable(std::array<double, 7> speeds);

		std::vector<NPC_Drop *> drops;
		std::string shop_name;
		std::string skill_name;
		std::vector<NPC_Shop_Trade_Item *> shop_trade;
		std::vector<NPC_Shop_Craft_Item *> shop_craft;
		std::vector<NPC_Learn_Skill *> skill_learn;
		std::list<NPC_Opponent *> damagelist;
		NPC_Citizenship *citizenship;
		NPC_Marriage *marriage;

		NPC *parent;

		bool alive;
		bool pet;
		bool attack_command;

		double dead_since;
		double last_act;
		double last_chat;
		double act_speed;

		int walk_idle_for;

		bool attack;
		bool temporary;
		bool ActAggressive;

        int hp;
        int tp;

		int totaldamage;
		int warntimer;
        int spelltimer;
        int talktimer;
        int maxtp;
        int mindam;
        int maxdam;
        int damageTaken;

        std::string name;

        int rebirth;
        int level;
        int exp;

        Character *killowner;

        std::list<Character_Item> inventory;

		Map *map;

		NPC(Map *map, short id, unsigned char x, unsigned char y, unsigned char spawn_type, short spawn_time, unsigned char index, bool temporary = false, bool pet = false, int spelltimer = 0);
		void LoadShopDrop();

		const ENF_Data& Data() const;

		void Spawn(NPC *parent = 0);
		void CalculateTNL();
        void CalculateStats();
        void PickupDrops();
        void CastSpells();
        void HealOwner();
		void Act();

		bool Walk(Direction);
		bool OpenInventory();
		bool AddItem(short item, int amount);
		void Effect(Character *from, int effect, int damage);
		void Damage(Character *from, int amount, int spell_id = -1, bool npckill = false);
		void RemoveFromView(Character *target);
		void Killed(Character *from, int amount, int spell_id = -1, bool npckill = false);
		void Die(bool show = true);

		void Attack(Character *target);
		void ShowDialog(std::string message);

		void FormulaVars(std::unordered_map<std::string, double> &vars, std::string prefix = "");

		~NPC();
};

#endif
