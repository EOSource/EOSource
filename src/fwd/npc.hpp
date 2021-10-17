#ifndef FWD_NPC_HPP_INCLUDED
#define FWD_NPC_HPP_INCLUDED

class NPC;

struct NPC_Opponent;
struct NPC_Drop;
struct NPC_Shop_Trade_Item;
struct NPC_Shop_Craft_Ingredient;
struct NPC_Shop_Craft_Item;
struct NPC_Citizenship;
struct NPC_Marriage;

enum InnUnsubscribeReply : unsigned char
{
	UNSUBSCRIBE_NOT_CITIZEN = 0,
	UNSUBSCRIBE_UNSUBSCRIBED = 1
};

enum SkillMasterReply : short
{
	SKILLMASTER_REMOVE_ITEMS = 1,
	SKILLMASTER_WRONG_CLASS = 2
};

#endif
