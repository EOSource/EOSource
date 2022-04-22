#include "npc.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "util.hpp"
#include "util/rpn.hpp"

#include "character.hpp"
#include "config.hpp"
#include "console.hpp"
#include "eoclient.hpp"
#include "eodata.hpp"
#include "map.hpp"
#include "packet.hpp"
#include "party.hpp"
#include "player.hpp"
#include "timer.hpp"
#include "quest.hpp"
#include "world.hpp"

static std::list<int> ExceptUnserialize(std::string serialized)
{
    std::list<int> list;

    std::size_t p = 0;
    std::size_t lastp = std::numeric_limits<std::size_t>::max();

    if (!serialized.empty() && *(serialized.end()-1) != ',')
    {
        serialized.push_back(',');
    }

    while ((p = serialized.find_first_of(',', p+1)) != std::string::npos)
    {
        list.push_back(util::to_int(serialized.substr(lastp + 1, p-lastp - 1)));
        lastp = p;
    }

    return list;
}

static double speed_table[8] = {0.9, 0.6, 1.3, 1.9, 3.7, 7.5, 15.0, 0.0};

void NPC::SetSpeedTable(std::array<double, 7> speeds)
{
    for (std::size_t i = 0; i < 7; ++i)
	{
        if (speeds[i] != 0.0)
			speed_table[i] = speeds[i];
	}
}

NPC::NPC(Map *map, short id, unsigned char x, unsigned char y, unsigned char spawn_type, short spawn_time, unsigned char index, bool temporary, bool pet, int spelltimer)
{
    this->marriage = 0;
	this->map = map;
	this->temporary = temporary;
	this->index = index;
	this->id = id;
	this->spawn_x = this->x = x;
	this->spawn_y = this->y = y;
	this->alive = false;
	this->attack = false;
	this->totaldamage = 0;
    this->owner = owner;
    this->pet = pet;
    this->spelltimer = 0;
    this->talktimer = 0;
    this->killowner = 0;
    this->maxtp = 0;
    this->mindam = 0;
    this->maxdam = 0;
    this->last_chat = 0;
    this->warntimer = 0;
    this->ActAggressive = false;

    if (this->tp < 0)
	    this->tp = 0;

	if (spawn_type > 7)
		spawn_type = 7;

	this->spawn_type = spawn_type;
	this->spawn_time = spawn_time;

	this->walk_idle_for = 0;

	if (spawn_type == 7)
	{
		this->direction = static_cast<Direction>(spawn_time & 0x03);
		this->spawn_time = 0;
	}
	else
	{
		this->direction = DIRECTION_DOWN;
	}

	this->parent = 0;
	this->citizenship = 0;

	this->pet = pet;
    this->attack_command = !pet;

	if (!pet)
	{
	    this->LoadShopDrop();
	}
}

void NPC::LoadShopDrop()
{
	this->drops.clear();
	this->shop_trade.clear();
	this->shop_craft.clear();
	this->skill_learn.clear();

	Config::iterator drops = map->world->drops_config.find(util::to_string(this->id));
	if (drops != map->world->drops_config.end())
	{
		std::vector<std::string> parts = util::explode(',', static_cast<std::string>((*drops).second));

		if (parts.size() > 1)
		{
			if (parts.size() % 4 != 0)
			{
				Console::Wrn("skipping invalid drop data for NPC #%i", id);
				return;
			}

			this->drops.resize(parts.size() / 4);

			for (std::size_t i = 0; i < parts.size(); i += 4)
			{
				NPC_Drop *drop(new NPC_Drop);

				drop->id = util::to_int(parts[i]);
				drop->min = util::to_int(parts[i+1]);
				drop->max = util::to_int(parts[i+2]);
				drop->chance = util::to_float(parts[i+3]);

				this->drops[i/4] = drop;
			}
		}
	}

	short shop_vend_id;

	if (int(map->world->shops_config["Version"]) < 2)
	{
		shop_vend_id = this->id;
	}
	else
	{
		shop_vend_id = this->Data().vendor_id;
	}

	if (this->Data().type == ENF::Type::Shop && shop_vend_id > 0)
	{
		this->shop_name = static_cast<std::string>(map->world->shops_config[util::to_string(shop_vend_id) + ".name"]);
		Config::iterator shops = map->world->shops_config.find(util::to_string(shop_vend_id) + ".trade");
		if (shops != map->world->shops_config.end())
		{
			std::vector<std::string> parts = util::explode(',', static_cast<std::string>((*shops).second));

			if (parts.size() > 1)
			{
				if (parts.size() % 3 != 0)
				{
					Console::Wrn("skipping invalid trade shop data for vendor #%i", shop_vend_id);
					return;
				}

				this->shop_trade.resize(parts.size() / 3);

				for (std::size_t i = 0; i < parts.size(); i += 3)
				{
					NPC_Shop_Trade_Item *item(new NPC_Shop_Trade_Item);
					item->id = util::to_int(parts[i]);
					item->buy = util::to_int(parts[i+1]);
					item->sell = util::to_int(parts[i+2]);

					if (item->buy != 0 && item->sell != 0 && item->sell > item->buy)
					{
						Console::Wrn("item #%i (vendor #%i) has a higher sell price than buy price.", item->id, shop_vend_id);
					}

					this->shop_trade[i/3] = item;
				}
			}
		}

		shops = this->map->world->shops_config.find(util::to_string(shop_vend_id) + ".craft");
		if (shops != this->map->world->shops_config.end())
		{
			std::vector<std::string> parts = util::explode(',', static_cast<std::string>((*shops).second));

			if (parts.size() > 1)
			{
				if (parts.size() % 9 != 0)
				{
					Console::Wrn("skipping invalid craft shop data for vendor #%i", shop_vend_id);
					return;
				}

				this->shop_craft.resize(parts.size() / 9);

				for (std::size_t i = 0; i < parts.size(); i += 9)
				{
					NPC_Shop_Craft_Item *item = new NPC_Shop_Craft_Item;
					std::vector<NPC_Shop_Craft_Ingredient *> ingredients;
					ingredients.resize(4);

					item->id = util::to_int(parts[i]);

					for (int ii = 0; ii < 4; ++ii)
					{
						NPC_Shop_Craft_Ingredient *ingredient = new NPC_Shop_Craft_Ingredient;
						ingredient->id = util::to_int(parts[i+1+ii*2]);
						ingredient->amount = util::to_int(parts[i+2+ii*2]);
						ingredients[ii] = ingredient;
					}

					item->ingredients = ingredients;

					this->shop_craft[i/9] = item;
				}
			}
		}
	}

	short skills_vend_id;

	if (int(map->world->skills_config["Version"]) < 2)
	{
		skills_vend_id = this->id;
	}
	else
	{
		skills_vend_id = this->Data().vendor_id;
	}

	if (this->Data().type == ENF::Type::Skills && skills_vend_id > 0)
	{
		this->skill_name = static_cast<std::string>(map->world->skills_config[util::to_string(skills_vend_id) + ".name"]);
		Config::iterator skills = this->map->world->skills_config.find(util::to_string(skills_vend_id) + ".learn");
		if (skills != this->map->world->skills_config.end())
		{
			std::vector<std::string> parts = util::explode(',', static_cast<std::string>((*skills).second));

			if (parts.size() > 1)
			{
				if (parts.size() % 14 != 0)
				{
					Console::Err("WARNING: skipping invalid skill learn data for vendor #%i", skills_vend_id);
					return;
				}

				this->skill_learn.resize(parts.size() / 14);

				for (std::size_t i = 0; i < parts.size(); i += 14)
				{
					NPC_Learn_Skill *skill = new NPC_Learn_Skill;

					skill->id = util::to_int(parts[i]);
					skill->cost = util::to_int(parts[i+1]);
					skill->levelreq = util::to_int(parts[i+2]);
					skill->classreq = util::to_int(parts[i+3]);

					skill->skillreq[0] = util::to_int(parts[i+4]);
					skill->skillreq[1] = util::to_int(parts[i+5]);
					skill->skillreq[2] = util::to_int(parts[i+6]);
					skill->skillreq[3] = util::to_int(parts[i+7]);

					skill->strreq = util::to_int(parts[i+8]);
					skill->intreq = util::to_int(parts[i+9]);
					skill->wisreq = util::to_int(parts[i+10]);
					skill->agireq = util::to_int(parts[i+11]);
					skill->conreq = util::to_int(parts[i+12]);
					skill->chareq = util::to_int(parts[i+13]);

					this->skill_learn[i/14] = skill;
				}
			}
		}
	}

	short home_vend_id;

	if (int(map->world->home_config["Version"]) < 2)
	{
		home_vend_id = this->id;
	}
	else
	{
		home_vend_id = this->Data().vendor_id;
	}

	this->citizenship = 0;

	if (this->Data().type == ENF::Type::Inn && home_vend_id > 0)
	{
		restart_loop:
		UTIL_FOREACH(this->map->world->home_config, hc)
		{
			std::vector<std::string> parts = util::explode('.', hc.first);

			if (parts.size() < 2)
			{
				continue;
			}

			if (!this->citizenship && parts[1] == "innkeeper" && util::to_int(hc.second) == home_vend_id)
			{
				this->citizenship = new NPC_Citizenship;
				this->citizenship->home = parts[0];
				Home* home = this->map->world->GetHome(this->citizenship->home);

				if (home)
					home->innkeeper_vend = this->Data().vendor_id;
				else
					Console::Wrn("Vendor #%i's innkeeper set on non-existent home: %s", home_vend_id, this->citizenship->home.c_str());

				// Restart the loop so questions/answers specified before the innkeeper option will load
				goto restart_loop;
			}
			else if (this->citizenship && this->citizenship->home == parts[0] && parts[1].substr(0, parts[1].length() - 1) == "question")
			{
				int index = parts[1][parts[1].length() - 1] - '1';

				if (index < 0 || index >= 3)
				{
					Console::Wrn("Exactly 3 questions must be specified for %s innkeeper vendor #%i", std::string(hc.second).c_str(), home_vend_id);
				}

				this->citizenship->questions[index] = static_cast<std::string>(hc.second);
			}
			else if (this->citizenship && this->citizenship->home == parts[0] && parts[1].substr(0, parts[1].length() - 1) == "answer")
			{
				int index = parts[1][parts[1].length() - 1] - '1';

				if (index < 0 || index >= 3)
				{
					Console::Wrn("Exactly 3 answers must be specified for %s innkeeper vendor #%i", std::string(hc.second).c_str(), home_vend_id);
				}

				this->citizenship->answers[index] = static_cast<std::string>(hc.second);
			}
		}
	}
}

const ENF_Data& NPC::Data() const
{
	return this->map->world->enf->Get(id);
}

void NPC::Spawn(NPC *parent)
{
	if (this->alive)
		return;

	if (this->Data().boss && !parent)
	{
		UTIL_FOREACH(this->map->npcs, npc)
		{
			if (npc->Data().child)
			{
				npc->Spawn(this);
			}
		}
	}

	if (parent)
	{
		this->parent = parent;
	}

	if (this->spawn_type < 7)
	{
		bool found = false;
		for (int i = 0; i < 200; ++i)
		{
			if (this->temporary && i == 0)
			{
				this->x = this->spawn_x;
				this->y = this->spawn_y;
			}
			else
			{
				this->x = util::rand(this->spawn_x-2, this->spawn_x+2);
				this->y = util::rand(this->spawn_y-2, this->spawn_y+2);
			}

			if (this->map->Walkable(this->x, this->y, true) && (i > 100 || !this->map->Occupied(this->x, this->y, Map::NPCOnly)))
			{
				this->direction = static_cast<Direction>(util::rand(0,3));
				found = true;
				break;
			}
		}

		if (!found)
		{
			Console::Wrn("An NPC on map %i at %i,%i is being placed by linear scan of spawn area (%s)", this->map->id, this->spawn_x, this->spawn_y, this->map->world->enf->Get(this->id).name.c_str());
			for (this->x = this->spawn_x-2; this->x <= spawn_x+2; ++this->x)
			{
				for (this->y = this->spawn_y-2; this->y <= this->spawn_y+2; ++this->y)
				{
					if (this->map->Walkable(this->x, this->y, true))
					{
						Console::Wrn("Placed at valid location: %i,%i", this->x, this->y);
						found = true;
						goto end_linear_scan;
					}
				}
			}
		}
		end_linear_scan:

		if (!found)
		{
			Console::Err("NPC couldn't spawn anywhere valid!");
		}
	}

	this->alive = true;
	this->hp = this->Data().hp;
	this->last_act = Timer::GetTime();
	this->act_speed = speed_table[this->spawn_type];

	PacketBuilder builder(PACKET_APPEAR, PACKET_REPLY, 8);
	builder.AddChar(0);
	builder.AddByte(255);
	builder.AddChar(this->index);
	builder.AddShort(this->id);
	builder.AddChar(this->x);
	builder.AddChar(this->y);
	builder.AddChar(this->direction);

	UTIL_FOREACH(this->map->characters, character)
	{
		if (character->InRange(this))
		{
			character->Send(builder);
		}
	}
}

int PetAntiGank(int minlevel ,int maxlevel)
{
	if (minlevel >= maxlevel)
	{
		return minlevel - maxlevel;
	}
	else
	{
		return maxlevel - minlevel;
	}
}

void NPC::CalculateTNL()
{
    bool level_up = false;

    while (this->owner->pet->level < int(this->map->world->config["MaxPetLevel"]) && this->owner->pet->exp >= this->map->world->exp_table[this->owner->pet->level +1])
    {
        level_up = true;
        ++this->level;
    }

    if (level_up)
    {
        this->owner->pet->ShowDialog(util::ucfirst(this->owner->SourceName()) + ", I am now level " + util::to_string(this->owner->pet->level));
        this->owner->StatusMsg("Your pet has leveled and is now level " + util::to_string(this->owner->pet->level));

        this->owner->pet->CalculateStats();
    }
}

void NPC::CalculateStats()
{
    int rebirth = this->owner->world->pets_config["RebirthLevel"];

    int mintp = this->owner->world->pets_config[util::to_string(this->id) + ".BaseMaxTP"];
    int maxtp = this->owner->world->pets_config[util::to_string(this->id) + ".TPPerLevel"];

    int mindam = this->owner->world->pets_config[util::to_string(this->id) + ".MinDmgPerLevel"];
    int maxdam = this->owner->world->pets_config[util::to_string(this->id) + ".MaxDmgPerLevel"];

    if (this->hp > this->Data().hp)
        this->hp = this->Data().hp;

    int minDamage = (mindam * this->level) + ((this->rebirth * rebirth) * mindam);
    int maxDamage = (maxdam * this->level) + ((this->rebirth * rebirth) * maxdam);

    if (this->pet && this->owner)
    {
        this->mindam = minDamage;
        this->maxdam = maxDamage;

        if (mintp <= 0)
            mintp = 20;

        this->maxtp = mintp + (this->level * maxtp) + ((rebirth * this->rebirth) * maxtp);
    }

    this->owner->SavePet();
}

void NPC::PickupDrops()
{
    if (this->talktimer == 0)
            this->talktimer = Timer::GetTime() + 1.50;

    UTIL_FOREACH(this->map->items, item)
    {
        unsigned int lockermax = int(this->owner->world->pets_config["MaxInventoryItems"]);

        if (util::path_length(this->x, this->y, item->x, item->y) < 4 && (item->owner == 0 || item->owner == this->owner->player->id))
        {
            if (this->owner->petdrops == true)
            {
                int drop_distance = util::path_length(item->x, item->y, this->x, this->y);

                if (drop_distance < 4 && item->owner == this->owner->player->id)
                {
                    if (this->map->world->pets_config[util::to_string(this->id) + ".AllowPickingUpDrops"])
                    {
                        int cap = this->map->world->config["MaxPetItem"];

                        if (this->inventory.size() < lockermax)
                        {
                            PacketBuilder builder;
                            builder.SetID(PACKET_ITEM, PACKET_GET);

                            if (item->id == 1)
                            {
                                this->owner->GiveItem(item->id, item->amount);
                            }
                            else
                            {
                                if (item->amount <= cap)
                                    this->AddItem(item->id, item->amount);
                                else if (item->amount > cap)
                                    this->AddItem(item->id, this->map->world->config["MaxPetItem"]);
                            }

                            this->owner->map->DelItem(item->uid, 0);

                            if (item->id != 1 && this->owner->pettalk && this->map->world->pets_config[util::to_string(this->id) + ".AllowTalking"])
                                this->ShowDialog(util::ucfirst(this->owner->SourceName()) + ", I found something for you! " + util::to_string(item->amount <= cap ? item->amount : cap) + " " + (item->id == 1 ? this->map->world->eif->Get(1).name : this->map->world->eif->Get(item->id).name));

                            break;
                        }
                        else
                        {
                            if (Timer::GetTime() > this->talktimer && this->owner->pettalk && this->map->world->pets_config[util::to_string(this->id) + ".AllowTalking"])
                            {
                                this->ShowDialog(util::ucfirst(this->owner->SourceName()) + ", I cannot carry anymore items!");
                                this->talktimer = Timer::GetTime() + 10.0;
                            }
                        }
                    }
                }
            }
        }
    }
}

void NPC::HealOwner()
{
    int owner_distance = util::path_length(this->owner->x, this->owner->y, this->x, this->y);

    if (this->map->world->pets_config[util::to_string(this->id) + ".AllowHealing"])
    {
        if (this->owner && owner_distance < 6 && this->owner->hp < this->owner->maxhp && this->owner->petheal == true)
        {
            if (this->owner->hp <= this->owner->maxhp * .60 && this->owner->pet->attack == false)
            {
                if ((this->level >= int(this->map->world->pets_config["HealLevelRequirement"]) || this->rebirth > 0) && this->rebirth >= int(this->map->world->pets_config["HealRebornRequirement"]))
                {
                    int heal = int(this->map->world->pets_config[util::to_string(this->id) + ".HealAmount"]);
                        int hpgain = heal + this->level * heal;

                    if (hpgain > 0)
                    {
                        ESF_Data& spell = this->owner->world->esf->Get(1); // Use this spell's TP cost.

                        if (this->owner->pet->tp >= spell.tp)
                        {
                            this->owner->pet->tp -= spell.tp;

                            // Give taken TP back to owner

                            if (this->owner->tp > spell.tp)
                                this->owner->tp += spell.tp;

                            this->owner->PacketRecover();

                            this->owner->Effect(9);

                            if (this->map->world->config["LimitDamage"])
                                hpgain = std::min(hpgain, this->owner->maxhp - this->owner->hp);

                            hpgain = std::max(hpgain, 0);

                            this->owner->hp += hpgain;
                            this->owner->hp = std::min(this->owner->hp, this->owner->maxhp);

                            if (!this->map->world->config["LimitDamage"])
                                this->owner->hp = std::min(this->owner->hp, this->owner->maxhp);

                            PacketBuilder builder(PACKET_RECOVER, PACKET_AGREE, 7);
                            builder.AddShort(this->owner->player->id);
                            builder.AddInt(hpgain);
                            builder.AddChar(int(double(this->owner->hp) / double(this->owner->maxhp) * 100.0));

                            UTIL_FOREACH(this->owner->map->characters, updatecharacter)
                            {
                                if (updatecharacter != this->owner && this->owner->InRange(updatecharacter))
                                    updatecharacter->Send(builder);
                            }

                            this->owner->Send(builder);

                            if (this->owner->party)
                                this->owner->party->UpdateHP(this->owner);

                            this->owner->PacketRecover();
                                this->CalculateStats();
                        }
                        else
                        {
                            if (Timer::GetTime() > this->warntimer && this->owner->pettalk && this->map->world->pets_config[util::to_string(this->id) + ".AllowTalking"])
                            {
                                this->ShowDialog("I don't have enough technique points to heal.");
                                this->warntimer = Timer::GetTime() + util::rand(30, 180);
                            }
                        }
                    }
                }
            }
        }
    }
}

void NPC::CastSpells()
{
    if (this->spelltimer == 0)
        this->spelltimer = Timer::GetTime() + 1.50;

    int owner_distance = util::path_length(this->owner->x, this->owner->y, this->x, this->y);

    if (this->map->world->pets_config[util::to_string(this->id) + ".AllowSpells"])
    {
        if (this->owner->mapid == int(this->map->world->pvp_config["PVPMap"]) && !this->map->world->pvp)
            return;

        if (owner_distance < 6 && this->owner->petspells == true && Timer::GetTime() > this->spelltimer)
        {
            if (this->level >= int(this->map->world->pets_config["SpellsLevelRequirement"]))
            {
                if (this->rebirth >= int(this->map->world->pets_config["SpellsRebornRequirement"]))
                {
                    UTIL_FOREACH(this->map->npcs, checkopp)
                    {
                        int npc_distance = util::path_length(this->x, this->y, checkopp->x, checkopp->y);

                        if ((checkopp->ActAggressive == true || checkopp->Data().type == ENF::Aggressive) && checkopp->alive && npc_distance > 3 && npc_distance < 7)
                        {
                            int spellid = int(this->map->world->pets_config[util::to_string(this->id) + ".CastSpell"]);
                            int amount = util::rand(this->Data().mindam + (this->level * 1) + (this->rebirth * int(this->map->world->pets_config["RebirthLevel"])) , this->Data().maxdam + (this->level * 2) + (this->rebirth * (int(this->map->world->pets_config["RebirthLevel"]) * 2)));
                            int limitamount = std::min(amount, int(checkopp->hp));

                            ESF_Data& spell = this->owner->world->esf->Get(spellid);

                            if (this->owner->pet->tp >= spell.tp)
                            {
                                this->owner->pet->tp -= spell.tp;

                                // Give the taken TP back to the player

                                if (this->owner->tp > spell.tp)
                                    this->owner->tp += spell.tp;

                                this->owner->PacketRecover();

                                if (static_cast<int>(this->map->world->config["LimitDamage"]))
                                    amount = limitamount;

                                if (spellid < 1 || std::size_t(spellid) > this->owner->world->esf->data.size())
                                    break;

                                this->map->SpellAttack(this->owner, checkopp, spellid);
                                    checkopp->damageTaken = checkopp->damageTaken + amount;

                                this->CalculateStats();
                                    this->spelltimer = Timer::GetTime() + 3.0;

                                return;
                            }
                            else
                            {
                                if (Timer::GetTime() > this->warntimer)
                                {
                                    this->ShowDialog("I don't have enough technique points to cast spells.");
                                    this->warntimer = Timer::GetTime() + util::rand(30, 180);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void NPC::Act()
{
    if (this->Data().type == ENF::Passive && this->hp != this->Data().hp && this->alive)
        this->ActAggressive = true;

	if (this->Data().child && !this->parent)
	{
		UTIL_FOREACH(this->map->npcs, npc)
		{
			if (npc->Data().boss)
			{
				this->parent = npc;
				break;
			}
		}
	}

	UTIL_FOREACH(this->damagelist, opponent)
    {
        if (opponent->attacker)
        {
            if (opponent->attacker->map != this->map || opponent->attacker->nowhere || opponent->last_hit < Timer::GetTime() - static_cast<double>(this->map->world->config["NPCBoredTimer"]))
            {
                if (this->killowner)
                {
                    if (opponent->attacker == this->killowner)
                        this->killowner = 0;
                }
            }
        }
    }

	this->last_act += double(util::rand(int(this->act_speed * 750.0), int(this->act_speed * 1250.0))) / 1000.0;

	if (this->spawn_type == 7)
		return;

	Character *attacker = 0;

	// NPCs casting spells

	if (this->map->world->config["NPCSpells"])
	{
        if (this->spelltimer == 0)
            this->spelltimer = Timer::GetTime() + 1.50;

        if (this->Data().unka > 0 && Timer::GetTime() > this->spelltimer && this->map->world->config["NPCSpells"])
        {
            std::vector<Character *> dcheck_chars;

            UTIL_FOREACH(this->map->characters, character)
            {
                const ESF_Data& spell = this->map->world->esf->Get(this->Data().unka);

                int amount = util::rand(this->Data().mindam + spell.mindam, this->Data().maxdam + spell.maxdam);
                int distance = util::path_length(character->x, character->y, this->x, this->y);
                int limitamount = std::min(amount, int(character->hp));

                if (distance > 1 && this->Data().unka > 0)
                {
                    if (distance <= int(this->map->world->config["NPCSpellDistance"]))
                    {
                        if (this->map->world->config["LimitDamage"])
                            amount = limitamount;

                        if (character->immune == false)
                            character->hp -= limitamount;

                        character->PacketRecover();

                        PacketBuilder builder(PACKET_AVATAR, PACKET_ADMIN, 12);
                        builder.AddShort(0);
                        builder.AddShort(character->player->id);
                        builder.AddThree(character->immune == false ? amount : 0);
                        builder.AddChar(character->direction);
                        builder.AddChar(util::clamp<int>(double(character->hp) / double(character->maxhp) * 100.0, 0, 100));
                        builder.AddChar(character->hp == 0);
                        builder.AddShort(this->Data().unka);

                        UTIL_FOREACH(this->map->characters, checkchar)
                        {
                            if (checkchar->InRange(character))
                                checkchar->Send(builder);
                        }

                        dcheck_chars.push_back(character);

                        this->spelltimer = Timer::GetTime() + 1.50;
                    }
                }
            }

            UTIL_FOREACH(dcheck_chars, character)
            {
                if (character->hp == 0)
                    character->DeathRespawn();
            }
        }
    }

    if (this->pet && this->Data().type == ENF::Quest && this->owner)
    {
        if (this->warntimer == 0)
            this->warntimer = Timer::GetTime() + 1.00;

        UTIL_FOREACH(this->map->npcs, npc)
        {
            std::list<int> except_list = ExceptUnserialize(this->owner->world->pets_config["AntiSpawnIDs"]);

            if (std::find(except_list.begin(), except_list.end(), this->owner->mapid) != except_list.end())
            {
                if (this->owner->HasPet)
                {
                    this->owner->PetTransfer();
                    this->owner->pettransfer = false;

                    this->owner->StatusMsg("Pets are not allowed here!");
                }
            }

            int owner_distance = util::path_length(this->owner->x, this->owner->y, this->x, this->y);

            this->PickupDrops();
            this->CastSpells();
            this->HealOwner();

            if (owner_distance > 8)
                this->owner->SummonPet(this->owner, this->owner->pet->name);

            int distance = util::path_length(npc->x, npc->y, this->x, this->y);
            int player_distance = util::path_length(this->owner->x, this->owner->y, this->x, this->y);

            if ((npc->Data().type == ENF::Aggressive && distance > 1 && distance < 6 && npc->alive && !attacker)
            || ((player_distance > 5 && !attacker) || (this->map->arena && distance > 1 && distance < 5 && npc->alive)))
            {
                int xdiff = this->x - npc->x;
                int ydiff = this->y - npc->y;

                int absxdiff = std::abs(xdiff);
                int absydiff = std::abs(ydiff);

                if ((absxdiff == 1 && absydiff == 0) || (absxdiff == 0 && absydiff == 1) || (absxdiff == 0 && absydiff == 0))
                {
                    return;
                }
                else if (absxdiff > absydiff)
                {
                    if (xdiff < 0)
                    {
                        this->direction = DIRECTION_RIGHT;
                    }
                    else
                    {
                        this->direction = DIRECTION_LEFT;
                    }
                }
                else
                {
                    if (ydiff < 0)
                    {
                        this->direction = DIRECTION_DOWN;
                    }
                    else
                    {
                        this->direction = DIRECTION_UP;
                    }
                }

                if (!this->Walk(this->direction))
                {
                    this->Walk(static_cast<Direction>(util::rand(0,3)));
                }

                return;
            }

            // Pets attacking NPCs.

            if (((npc->ActAggressive == true || npc->Data().type == ENF::Aggressive) || (this->owner->map->pk == true && npc->pet && npc->Data().type == ENF::Quest && npc != this->owner->pet)) && this->owner->petattack == true && distance < 2 && npc->alive && this->map->world->pets_config[util::to_string(this->id) + ".AllowMelee"])
            {
                if (this->owner->mapid == int(this->map->world->pvp_config["PVPMap"]) && !this->map->world->pvp)
                    return;

                int amount = util::rand(this->Data().mindam + this->mindam, this->Data().maxdam + static_cast<int>(this->map->world->config["NPCAdjustMaxDam"]) + this->maxdam);

                if (distance < 2)
                {
                    int xdiff = this->x - npc->x;
                    int ydiff = this->y - npc->y;

                    if (std::abs(xdiff) > std::abs(ydiff))
                    {
                        if (xdiff < 0)
                        {
                            this->direction = DIRECTION_RIGHT;
                        }
                        else
                        {
                            this->direction = DIRECTION_LEFT;
                        }
                    }
                    else
                    {
                        if (ydiff < 0)
                        {
                            this->direction = DIRECTION_DOWN;
                        }
                        else
                        {
                            this->direction = DIRECTION_UP;
                        }
                    }

                    PacketBuilder builder(PACKET_NPC, PACKET_PLAYER, 18);
                    builder.AddByte(255);
                    builder.AddChar(this->index);
                    builder.AddChar(1 + (npc->hp == 0));
                    builder.AddChar(this->direction);
                    builder.AddShort(0);
                    builder.AddThree(amount);
                    builder.AddThree(util::clamp<int>(double(npc->hp) / double(npc->Data().hp) * 100.0, 0, 100));
                    builder.AddByte(255);
                    builder.AddByte(255);

                    UTIL_FOREACH(this->map->characters, character)
                    {
                        if (character->InRange(this))
                        {
                            character->Send(builder);
                        }
                    }
                }

                double rand = util::rand(0.0, 1.0);
                bool critical = std::abs(int(npc->direction) - this->owner->direction) != 2 || rand < static_cast<double>(this->map->world->config["CriticalRate"]);

                std::unordered_map<std::string, double> formula_vars;

                this->FormulaVars(formula_vars);
                npc->FormulaVars(formula_vars, "target_");

                formula_vars["modifier"] = 1.0 / static_cast<double>(this->map->world->config["MobRate"]);
                formula_vars["damage"] = amount;
                formula_vars["critical"] = critical;

                amount = rpn_eval(rpn_parse(this->map->world->formulas_config["damage"]), formula_vars);
                double hit_rate = rpn_eval(rpn_parse(this->map->world->formulas_config["hit_rate"]), formula_vars);

                if (rand > hit_rate)
                    amount = 0;

                amount = std::max(amount, 0);

                int limitamount = std::min(amount, int(npc->hp));

                if (this->map->world->config["LimitDamage"])
                    amount = limitamount;

                npc->Damage(this->owner, amount, -1, true);
                npc->damageTaken = npc->damageTaken + amount;

                return;
            }
        }

        // Pets attacking players in PK

        if (this->map->pk == true)
        {
            if (this->owner->petattack == true)
            {
                UTIL_FOREACH(this->map->characters, character)
                {
                    int distance = util::path_length(character->x, character->y, this->x, this->y);

                    if (distance < 2)
                    {
                        if (character != this->owner)
                        {
                            this->Attack(character);
                            return;
                        }
                    }
                }
            }
        }
    }

    if (this->pet)
        attacker = this->owner;

    unsigned char attacker_distance = static_cast<int>(this->map->world->config["NPCChaseDistance"]);
    unsigned short attacker_damage = 0;

    if (this->Data().type == ENF::Passive || this->Data().type == ENF::Aggressive)
	{
		UTIL_FOREACH(this->damagelist, opponent)
		{
			if (opponent->attacker->map != this->map || opponent->attacker->nowhere || opponent->last_hit < Timer::GetTime() - static_cast<double>(this->map->world->config["NPCBoredTimer"]))
			{
                this->ActAggressive = false;
				continue;
			}

			int distance = util::path_length(opponent->attacker->x, opponent->attacker->y, this->x, this->y);

			if ((distance < attacker_distance) || (distance == attacker_distance && opponent->damage > attacker_damage))
			{
				attacker = opponent->attacker;
				attacker_damage = opponent->damage;
				attacker_distance = distance;
			}
		}

		if (this->parent)
		{
			UTIL_FOREACH(this->parent->damagelist, opponent)
			{
				if (opponent->attacker->map != this->map || opponent->attacker->nowhere || opponent->last_hit < Timer::GetTime() - static_cast<double>(this->map->world->config["NPCBoredTimer"]))
				{
                    this->ActAggressive = false;
					continue;
				}

				int distance = util::path_length(opponent->attacker->x, opponent->attacker->y, this->x, this->y);

				if ((distance < attacker_distance) || (distance == attacker_distance && opponent->damage > attacker_damage))
				{
					attacker = opponent->attacker;
					attacker_damage = opponent->damage;
					attacker_distance = distance;
				}
			}
		}
	}

	// Sets closest player as attacker for this npc

    if (this->Data().type == ENF::Aggressive || (this->parent && attacker))
    {
        Character *closest = 0;

        unsigned char closest_distance = static_cast<int>(this->map->world->config["NPCChaseDistance"]);

        if (attacker)
        {
            closest = attacker;
            closest_distance = std::min(closest_distance, attacker_distance);
        }

        UTIL_FOREACH(this->map->characters, character)
        {
            int distance = util::path_length(character->x, character->y, this->x, this->y);

            if (distance < closest_distance)
            {
                if (this->pet)
                {
                    if (this->attack_command)
                    {
                        if (this->owner != character)
                        {
                            closest = character;
                        }
                    }
                    else
                    {
                        closest = this->owner;
                    }
                }
                else
                {
                    if (!character->hidden)
                    {
                        closest = character;
                    }
                }

                closest_distance = distance;
            }
        }

        if (closest)
        {
            attacker = closest;
        }
    }

    // NPCs attacking pets.

	if ((this->Data().type == ENF::Aggressive || this->ActAggressive == true) && this->alive && attacker)
    {
        if (attacker->pet)
        {
            if (attacker->HasPet && attacker->pet->alive)
			{
                int xdiff = this->x - attacker->pet->x;
                int ydiff = this->y - attacker->pet->y;

                int distance = util::path_length(attacker->pet->x, attacker->pet->y, this->x, this->y);

                if (attacker->pet->Data().type == ENF::Quest && attacker->pet->alive && util::path_length(attacker->x, attacker->y, this->x, this->y) > distance)
                {
                    if (distance > 1 && distance < 5)
                    {
                        int absxdiff = std::abs(xdiff);
                        int absydiff = std::abs(ydiff);

                        if ((absxdiff == 1 && absydiff == 0) || (absxdiff == 0 && absydiff == 1) || (absxdiff == 0 && absydiff == 0))
                        {
                            return;
                        }

                        else if (absxdiff > absydiff)
                        {
                            if (xdiff < 0)
                            {
                                this->direction = DIRECTION_RIGHT;
                            }
                            else
                            {
                                this->direction = DIRECTION_LEFT;
                            }
                        }
                        else
                        {
                            if (ydiff < 0)
                            {
                                this->direction = DIRECTION_DOWN;
                            }
                            else
                            {
                                this->direction = DIRECTION_UP;
                            }
                        }

                        if (!this->Walk(this->direction))
                        {
                            this->Walk(static_cast<Direction>(util::rand(0,3)));
                        }

                        return;
                    }

                    if (distance > 0 && distance < 2)
                    {
                        if (std::abs(xdiff) > std::abs(ydiff))
                        {
                            if (xdiff < 0)
                            {
                                this->direction = DIRECTION_RIGHT;
                            }
                            else
                            {
                                this->direction = DIRECTION_LEFT;
                            }
                        }
                        else
                        {
                            if (ydiff < 0)
                            {
                                this->direction = DIRECTION_DOWN;
                            }
                            else
                            {
                                this->direction = DIRECTION_UP;
                            }
                        }

                        int amount = util::rand(this->Data().mindam, this->Data().maxdam + static_cast<int>(this->map->world->config["NPCAdjustMaxDam"]));

                        PacketBuilder builder(PACKET_NPC, PACKET_PLAYER);
                        builder.AddByte(255);
                        builder.AddChar(this->index);
                        builder.AddChar(1 + (attacker->hp == 0));
                        builder.AddChar(this->direction);
                        builder.AddShort(0);
                        builder.AddThree(amount);
                        builder.AddThree(util::clamp<int>(double(attacker->hp) / double(attacker->maxhp) * 100.0, 0, 100));
                        builder.AddByte(255);
                        builder.AddByte(255);

                        UTIL_FOREACH(this->map->characters, character)
                        {
                            if (character->InRange(this))
                            {
                                character->Send(builder);
                            }
                        }

                        double rand = util::rand(0.0, 1.0);
                        bool critical = std::abs(int(attacker->pet->direction) - attacker->direction) != 2 || rand < static_cast<double>(this->map->world->config["CriticalRate"]);

                        std::unordered_map<std::string, double> formula_vars;

                        attacker->FormulaVars(formula_vars);
                        attacker->pet->FormulaVars(formula_vars, "target_");
                        formula_vars["modifier"] = this->map->world->config["MobRate"];
                        formula_vars["damage"] = amount;
                        formula_vars["critical"] = critical;

                        amount = rpn_eval(rpn_parse(this->map->world->formulas_config["damage"]), formula_vars);
                        double hit_rate = rpn_eval(rpn_parse(this->map->world->formulas_config["hit_rate"]), formula_vars);

                        if (rand > hit_rate)
                            amount = 0;

                        amount = std::max(amount, 0);

                        int limitamount = std::min(amount, int(attacker->pet->hp));

                        if (this->map->world->config["LimitDamage"])
                            amount = limitamount;

                        attacker->pet->Damage(attacker, amount, -1, true);

                        return;
                    }
                }
            }
        }
    }

    // NPCs random walking.

    if ((attacker && !this->pet) || (this->pet && attacker == this->owner && util::path_length(attacker->x, attacker->y, this->x, this->y) > 3))
    {
        int xdiff = this->x - attacker->x;
        int ydiff = this->y - attacker->y;

        int absxdiff = std::abs(xdiff);
        int absydiff = std::abs(ydiff);

        if ((absxdiff == 1 && absydiff == 0) || (absxdiff == 0 && absydiff == 1) || (absxdiff == 0 && absydiff == 0))
        {
            if (attacker != this->owner)
            {
                this->Attack(attacker);
            }

            return;
        }
        else if (absxdiff > absydiff )
        {
            if (xdiff < 0)
            {
                this->direction = DIRECTION_RIGHT;
            }
            else
            {
                this->direction = DIRECTION_LEFT;
            }
        }
        else
        {
            if (ydiff < 0)
            {
                this->direction = DIRECTION_DOWN;
            }
            else
            {
                this->direction = DIRECTION_UP;
            }
        }

        if (!this->Walk(this->direction))
        {
            this->Walk(static_cast<Direction>(util::rand(0,3)));
        }
    }
    else
    {
        int act;

        if (this->walk_idle_for == 0)
        {
            act = util::rand(1,10);
        }
        else
        {
            --this->walk_idle_for;
            act = 11;
        }

        if (act >= 1 && act <= 6)
        {
            this->Walk(this->direction);
        }
        else if (act >= 7 && act <= 9)
        {
            this->Walk(static_cast<Direction>(util::rand(0,3)));
        }
        else if (act == 10)
        {
            this->walk_idle_for = util::rand(1,4);
        }
    }
}

bool NPC::OpenInventory()
{
    this->owner->petinv_open = true;

    PacketBuilder reply(PACKET_LOCKER, PACKET_OPEN, 2 + this->owner->pet->inventory.size() * 5);
    reply.AddChar(0);
    reply.AddChar(0);

    UTIL_FOREACH(this->owner->pet->inventory, item)
    {
        reply.AddShort(item.id);
        reply.AddThree(item.amount);
    }

    this->owner->Send(reply);

    return true;
}

bool NPC::Walk(Direction direction)
{
	return this->map->Walk(this, direction);
}

bool NPC::AddItem(short item, int amount)
{
    if (amount <= 0)
		return false;

	if (item <= 0 || static_cast<std::size_t>(item) >= this->map->world->eif->data.size())
		return false;

	UTIL_IFOREACH(this->inventory, it)
	{
		if (it->id == item)
		{
			if (it->amount + amount < 0)
				return false;

			it->amount += amount;
			it->amount = std::min<int>(it->amount, this->map->world->config["MaxPetItem"]);

			return true;
		}
	}

	Character_Item newitem;

	newitem.id = item;
	newitem.amount = amount;

	this->inventory.push_back(newitem);

	return true;
}

void NPC::Effect(Character *from, int effect, int damage)
{
    UTIL_FOREACH(this->map->characters, character)
    {
        PacketBuilder builder;
        builder.SetID(PACKET_CAST, PACKET_REPLY);
        builder.AddShort(effect);
        builder.AddShort(from->player->id);
        builder.AddChar(from->player->character->direction);
        builder.AddShort(this->index);
        builder.AddThree(damage);
        builder.AddShort(util::clamp<int>(double(this->hp) / double(this->Data().hp) * 100.0, 0, 100));
        builder.AddShort(0);

        if (character->InRange(this))
        {
            character->Send(builder);
        }
    }
}

// Players attacking NPCs.

void NPC::Damage(Character *from, int amount, int spell_id, bool npckill)
{
    int ksmode = util::to_int(from->world->config["KSProtectMode"]);

    bool ksprotected = ((this->Data().vendor_id == 1 && ksmode >= 2)
    || (this->Data().vendor_id == 0 && ksmode >= 1)) &&
    int(from->admin) < util::to_int(from->world->admin_config["ksoverride"]);

    for (int i = 0 ; i < int(this->map->world->effects_config["WAmount"]) ; i++)
    {
        int itemid = this->map->world->effects_config[util::to_string(i+1) + ".ItemID"];
        int effectid = this->map->world->effects_config[util::to_string(i+1) + ".EffectID"];

        if (from->paperdoll[Character::Weapon] == itemid)
        {
            if (!npckill)
                this->Effect(from, effectid, amount);

            from->PacketRecover();
        }
    }

     if (ksprotected && this->killowner)
     {
        if (this->killowner != from)
        {
            ksprotected = true;
        }
        else
        {
            ksprotected = false;
        }
    }
    else
    {
        ksprotected = false;
    }

    if (from->party && this->killowner)
    {
        UTIL_FOREACH(from->party->members, member)
        {
            if (member == this->killowner)
            {
                ksprotected = false;
                break;
            }
        }
    }

	int limitamount = std::min(this->hp, amount);

	if (this->map->world->config["LimitDamage"])
		amount = !ksprotected ? limitamount : 0;

	amount = !ksprotected ? amount : 0;

    this->hp -= amount;

	if (this->totaldamage + limitamount > this->totaldamage)
	{
	    this->totaldamage += limitamount;
	}

	bool found = false;

	UTIL_FOREACH(this->damagelist, checkopp)
    {
        if (checkopp->attacker)
        {
            if (checkopp->attacker == from && !ksprotected)
            {
                found = true;

                if (checkopp->damage + limitamount > checkopp->damage)
                    checkopp->damage += limitamount;

                checkopp->last_hit = Timer::GetTime();
            }
        }
    }

    NPC_Opponent *opponent(new NPC_Opponent);

    if (!found && !ksprotected)
    {
        opponent->attacker = from;
        opponent->damage = limitamount;
        opponent->last_hit = Timer::GetTime();
        this->damagelist.push_back(opponent);
        opponent->attacker->unregister_npc.push_back(this);
    }

	if (this->hp > 0)
    {
        if (!ksprotected && !this->killowner)
            this->killowner = from;

        PacketBuilder builder(spell_id == -1 ? PACKET_NPC : PACKET_CAST, PACKET_REPLY, 14);

        if (spell_id != -1)
            builder.AddShort(spell_id);

        if (npckill)
        {
            builder.SetID(PACKET_NPC, PACKET_REPLY);
            builder.AddShort(0);
            builder.AddChar(from->direction);
            builder.AddShort(this->index);
            builder.AddThree(amount);
            builder.AddShort(int(double(this->hp) / double(this->Data().hp) * 100.0));
        }
        else
        {
            builder.AddShort(from->player->id);
            builder.AddChar(from->direction);
            builder.AddShort(this->index);
            builder.AddThree(amount);
            builder.AddShort(util::clamp<int>(double(this->hp) / double(this->Data().hp) * 100.0, 0, 100));
        }

        if (spell_id != -1)
            builder.AddShort(from->tp);

        UTIL_FOREACH(this->map->characters, character)
        {
            if (character->InRange(this) && character != from)
            {
                character->Send(builder);
            }
        }

        from->Send(builder.AddChar(ksprotected ? 2 : 1));
    }
    else
    {
        std::string message = this->map->world->message_config[util::to_string(this->id) + ".KillMessage"];
            message = from->ReplaceStrings(from, message);

        if (message != "0")
            from->world->ServerMsg(message);

        if (!from->HasAchievement(std::string(this->map->world->achievements_config["FirstKill.Achievement." + util::to_string(this->id)])))
        {
            if (std::string(this->map->world->achievements_config["FirstKill.Achievement." + util::to_string(this->id)]) != "0")
            {
                from->GiveEXP(std::min(std::max(int(this->map->world->achievements_config["FirstKill.Reward." + util::to_string(this->id)]), 0), int(from->world->config["MaxExp"])));

                if (std::string(this->map->world->achievements_config["FirstKill.Title." + util::to_string(this->id)]) != "0")
                    from->title = std::string(this->map->world->achievements_config["FirstKill.Title." + util::to_string(this->id)]);

                if (std::string(this->map->world->achievements_config["FirstKill.Description." + util::to_string(this->id)]) != "0")
                    from->ServerMsg(std::string(this->map->world->achievements_config["FirstKill.Description." + util::to_string(this->id)]));

                if (std::string(this->map->world->achievements_config["FirstKill.Announcement." + util::to_string(this->id)]) == "yes")
                    from->world->ServerMsg(util::ucfirst(from->SourceName()) + " earned the achievement '" + std::string(this->map->world->achievements_config["FirstKill.Achievement." + util::to_string(this->id)]) + "'.");

                from->NewAchievement(std::string(this->map->world->achievements_config["FirstKill.Achievement." + util::to_string(this->id)]));
            }
        }

        double exprate = this->map->world->config["ExpRate"];

        int reward = int(std::ceil(double(this->Data().exp) * exprate));
        int boosted = int(std::ceil(double(this->Data().exp) * (from->boostexp == 0 ? 1 : from->boostexp)));

        if (!from->party)
        {
            if (from->member == 1)
                from->GiveEXP(std::min(std::max(int(reward + (reward * double(this->map->world->members_config["MemberEXPAmountL1"]))), 0), int(this->map->world->config["MaxExp"])));
            else if (from->member == 2)
                from->GiveEXP(std::min(std::max(int(reward + (reward * double(this->map->world->members_config["MemberEXPAmountL2"]))), 0), int(this->map->world->config["MaxExp"])));
            else if (from->member == 3)
                from->GiveEXP(std::min(std::max(int(reward + (reward * double(this->map->world->members_config["MemberEXPAmountL3"]))), 0), int(this->map->world->config["MaxExp"])));

            if (from->boosttimer > 0 && from->boostexp > 0)
                from->GiveEXP(std::min(std::max(int(boosted), 0), int(this->map->world->config["MaxExp"])));
        }

        if (this->pet)
        {
            this->owner->StatusMsg("Your pet has died!");

            if (this->owner->HasPet)
                this->owner->SavePet();

            this->owner->HasPet = false;
        }

        if (npckill)
        {
            this->Killed(from, amount, spell_id, true);
        }
        else
        {
            this->Killed(from, amount, spell_id);
                return;
        }
    }
}

void NPC::RemoveFromView(Character *target)
{
	PacketBuilder builder(PACKET_NPC, PACKET_PLAYER, 7);
	builder.AddChar(this->index);

	if (target->x > 200 && target->y > 200)
	{
		builder.AddChar(0);
		builder.AddChar(0);
	}
	else
	{
		builder.AddChar(252);
		builder.AddChar(252);
	}

	builder.AddChar(0);
	builder.AddByte(255);
	builder.AddByte(255);
	builder.AddByte(255);

	PacketBuilder reply(PACKET_NPC, PACKET_SPEC, 5);
	reply.AddShort(0);
	reply.AddChar(0);
	reply.AddShort(this->index);

	target->Send(builder);
	target->Send(reply);
}

void NPC::Killed(Character *from, int amount, int spell_id, bool npckill)
{
	int boosted = int(std::ceil(double(this->map->world->config["DropRate"]) * (from->boostdrop == 0 ? 1 : from->boostdrop)));

	double droprate = this->map->world->config["DropRate"];
	double exprate = this->map->world->config["ExpRate"];

	int sharemode = this->map->world->config["ShareMode"];
	int partysharemode = this->map->world->config["PartyShareMode"];

	std::set<Party *> parties;

	int most_damage_counter = 0;
	Character *most_damage = 0;

	this->alive = false;

	this->dead_since = int(Timer::GetTime());

	std::vector<NPC_Drop *> drops;
	NPC_Drop *drop = 0;

	UTIL_FOREACH(this->drops, checkdrop)
	{
	    if (from->boosttimer == 0 && from->boostdrop == 0)
        {
            if (util::rand(0.0, 100.0) <= checkdrop->chance * droprate)
                drops.push_back(checkdrop);
        }
        else
        {
            if (util::rand(0.0, 100.0) <= checkdrop->chance * boosted)
                drops.push_back(checkdrop);
        }
	}

	if (drops.size() > 0)
		drop = drops[util::rand(0, drops.size()-1)];

	if (sharemode == 1)
	{
		UTIL_FOREACH(this->damagelist, opponent)
		{
			if (opponent->damage > most_damage_counter)
			{
				most_damage_counter = opponent->damage;
				most_damage = opponent->attacker;
			}
		}
	}

	int dropuid = 0;
	int dropid = 0;
	int dropamount = 0;

	Character* drop_winner = 0;

	if (drop)
	{
		dropuid = this->map->GenerateItemID();
		dropid = drop->id;
		dropamount = util::rand(drop->min, drop->max);

		std::shared_ptr<Map_Item> newitem(std::make_shared<Map_Item>(dropuid, dropid, dropamount, this->x, this->y, from->player->id, Timer::GetTime() + static_cast<int>(this->map->world->config["ProtectNPCDrop"])));

		this->map->items.push_back(newitem);

		switch (sharemode)
		{
			case 0:
            drop_winner = from;
            break;

			case 1:
            drop_winner = most_damage;
            break;

			case 2:
			{
				int rewarded_hp = util::rand(0, this->totaldamage - 1);

				int count_hp = 0;

				UTIL_FOREACH(this->damagelist, opponent)
				{
					if (opponent->attacker->InRange(this))
					{
						if (rewarded_hp >= count_hp && rewarded_hp < opponent->damage)
						{
                            drop_winner = opponent->attacker;
							break;
						}

						count_hp += opponent->damage;
					}
				}
			}
            break;

			case 3:
			{
				int rand = util::rand(0, this->damagelist.size() - 1);
				int i = 0;

				UTIL_FOREACH(this->damagelist, opponent)
				{
					if (opponent->attacker->InRange(this))
					{
						if (rand == i++)
						{
                            drop_winner = opponent->attacker;
							break;
						}
					}
				}
			}
            break;
		}
	}

	if (drop_winner)
		this->map->items.back()->owner = drop_winner->player->id;

	UTIL_FOREACH(this->map->characters, character)
	{
		std::list<NPC_Opponent *>::iterator findopp = this->damagelist.begin();

		for (; findopp != this->damagelist.end() && (*findopp)->attacker != character; ++findopp);

		if (findopp != this->damagelist.end() || character->InRange(this))
		{
			bool level_up = false;

			PacketBuilder builder(spell_id == -1 ? PACKET_NPC : PACKET_CAST, PACKET_SPEC, 26);

			if (this->Data().exp != 0)
			{
				if (findopp != this->damagelist.end())
				{
					int reward;

					switch (sharemode)
					{
						case 0:
                        if (character == from)
                        {
                            reward = int(std::ceil(double(this->Data().exp) * exprate));

                            if (reward > 0)
                            {
                                if (partysharemode)
                                {
                                    if (character->party)
                                    {
                                        character->party->ShareEXP(reward, partysharemode, this->map);
                                    }
                                    else
                                    {
                                        if (character->member == 0 && (character->boosttimer == 0 && character->boostexp == 0))
                                            character->GiveEXP(std::min(std::max(reward, 0), int(character->world->config["MaxExp"])));

                                        if (character->HasPet)
                                        {
                                            if (this->damageTaken >= (this->Data().hp * 0.10))
                                            {
                                                character->pet->exp += std::min(std::max(reward, 0), int(character->world->config["MaxExp"]));
                                                character->pet->exp = std::min(character->pet->exp, util::to_int(character->world->config["MaxExp"]));

                                                character->pet->CalculateTNL();
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    if (character->member == 0 && (character->boosttimer == 0 && character->boostexp == 0))
                                            character->GiveEXP(std::min(std::max(reward, 0), int(character->world->config["MaxExp"])));

                                    if (character->HasPet)
                                    {
                                        if (this->damageTaken >= (this->Data().hp * 0.10))
                                        {
                                            character->pet->exp += std::min(std::max(reward, 0), int(character->world->config["MaxExp"]));
                                            character->pet->exp = std::min(character->pet->exp, util::to_int(character->world->config["MaxExp"]));

                                            character->pet->CalculateTNL();
                                        }
                                    }
                                }
                            }
                        }
                        break;

						case 1:
                        if (character == most_damage)
                        {
                            reward = int(std::ceil(double(this->Data().exp) * exprate));

                            if (reward > 0)
                            {
                                if (partysharemode)
                                {
                                    if (character->party)
                                    {
                                        character->party->ShareEXP(reward, partysharemode, this->map);
                                    }
                                    else
                                    {
                                        if (character->member == 0 && (character->boosttimer == 0 && character->boostexp == 0))
                                            character->GiveEXP(std::min(std::max(reward, 0), int(character->world->config["MaxExp"])));

                                        if (character->HasPet)
                                        {
                                            if (this->damageTaken >= (this->Data().hp * 0.10))
                                            {
                                                character->pet->exp += std::min(std::max(reward, 0), int(character->world->config["MaxExp"]));
                                                character->pet->exp = std::min(character->pet->exp, util::to_int(character->world->config["MaxExp"]));

                                                character->pet->CalculateTNL();
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    if (character->member == 0 && (character->boosttimer == 0 && character->boostexp == 0))
                                            character->GiveEXP(std::min(std::max(reward, 0), int(character->world->config["MaxExp"])));

                                    if (character->HasPet)
                                    {
                                        if (this->damageTaken >= (this->Data().hp * 0.10))
                                        {
                                            character->pet->exp += std::min(std::max(reward, 0), int(character->world->config["MaxExp"]));
                                            character->pet->exp = std::min(character->pet->exp, util::to_int(character->world->config["MaxExp"]));

                                            character->pet->CalculateTNL();
                                        }
                                    }
                                }
                            }
                        }
                        break;

						case 2:
                        reward = int(std::ceil(double(this->Data().exp) * exprate * (double((*findopp)->damage) / double(this->totaldamage))));

                        if (reward > 0)
                        {
                            if (partysharemode)
                            {
                                if (character->party)
                                {
                                    character->party->temp_expsum += reward;
                                    parties.insert(character->party);
                                }
                                else
                                {
                                    if (character->member == 0 && (character->boosttimer == 0 && character->boostexp == 0))
                                            character->GiveEXP(std::min(std::max(reward, 0), int(character->world->config["MaxExp"])));

                                    if (character->HasPet)
                                    {
                                        if (this->damageTaken >= (this->Data().hp * 0.10))
                                        {
                                            character->pet->exp += std::min(std::max(reward, 0), int(character->world->config["MaxExp"]));
                                            character->pet->exp = std::min(character->pet->exp, util::to_int(character->world->config["MaxExp"]));

                                            character->pet->CalculateTNL();
                                        }
                                    }
                                }
                            }
                            else
                            {
                                if (character->member == 0 && (character->boosttimer == 0 && character->boostexp == 0))
                                            character->GiveEXP(std::min(std::max(reward, 0), int(character->world->config["MaxExp"])));

                                if (character->HasPet)
                                {
                                    if (this->damageTaken >= (this->Data().hp * 0.10))
                                    {
                                        character->pet->exp += std::min(std::max(reward, 0), int(character->world->config["MaxExp"]));
                                        character->pet->exp = std::min(character->pet->exp, util::to_int(character->world->config["MaxExp"]));

                                        character->pet->CalculateTNL();
                                    }
                                }
                            }
                        }
                        break;

						case 3:
                        reward = int(std::ceil(double(this->Data().exp) * exprate * (double(this->damagelist.size()) / 1.0)));

                        if (reward > 0)
                        {
                            if (partysharemode)
                            {
                                if (character->party)
                                {
                                    character->party->temp_expsum += reward;
                                }
                                else
                                {
                                    if (character->member == 0 && (character->boosttimer == 0 && character->boostexp == 0))
                                            character->GiveEXP(std::min(std::max(reward, 0), int(character->world->config["MaxExp"])));

                                    if (character->HasPet)
                                    {
                                        if (this->damageTaken >= (this->Data().hp * 0.10))
                                        {
                                            character->pet->exp += std::min(std::max(reward, 0), int(character->world->config["MaxExp"]));
                                            character->pet->exp = std::min(character->pet->exp, util::to_int(character->world->config["MaxExp"]));

                                            character->pet->CalculateTNL();
                                        }
                                    }
                                }
                            }
                            else
                            {
                                if (character->member == 0 && (character->boosttimer == 0 && character->boostexp == 0))
                                            character->GiveEXP(std::min(std::max(reward, 0), int(character->world->config["MaxExp"])));

                                if (character->HasPet)
                                {
                                    if (this->damageTaken >= (this->Data().hp * 0.10))
                                    {
                                        character->pet->exp += std::min(std::max(reward, 0), int(character->world->config["MaxExp"]));
                                        character->pet->exp = std::min(character->pet->exp, util::to_int(character->world->config["MaxExp"]));

                                        character->pet->CalculateTNL();
                                    }
                                }
                            }
                        }
                        break;
					}

					character->exp = std::min(character->exp, static_cast<int>(this->map->world->config["MaxExp"]));

					while (character->level < static_cast<int>(this->map->world->config["MaxLevel"]) && character->exp >= this->map->world->exp_table[character->level+1])
					{
						level_up = true;
						++character->level;
						character->statpoints += static_cast<int>(this->map->world->config["StatPerLevel"]);
						character->skillpoints += static_cast<int>(this->map->world->config["SkillPerLevel"]);
						character->CalculateStats();
					}

					if (level_up)
					{
                        if (from->party)
                        {
                            UTIL_FOREACH(from->party->members, member)
                            {
                                from->party->RefreshMembers(member->player->character);
                            }
                        }

						builder.SetID(spell_id == -1 ? PACKET_NPC : PACKET_CAST, PACKET_ACCEPT);
						builder.ReserveMore(33);
					}

					if (!from->HasAchievement(std::string(this->map->world->achievements_config["ReachLevel.Achievement." + util::to_string(from->level)])))
                    {
                        if (std::string(this->map->world->achievements_config["ReachLevel.Achievement." + util::to_string(from->level)]) != "0")
                        {
                            from->GiveEXP(std::min(std::max(int(this->map->world->achievements_config["ReachLevel.Reward." + util::to_string(from->level)]), 0), int(from->world->config["MaxExp"])));

                            if (std::string(this->map->world->achievements_config["ReachLevel.Description." + util::to_string(from->level)]) != "0")
                                from->ServerMsg(std::string(this->map->world->achievements_config["ReachLevel.Description." + util::to_string(from->level)]));

                            if (std::string(this->map->world->achievements_config["ReachLevel.Announcement." + util::to_string(from->level)]) == "yes")
                                from->world->ServerMsg(util::ucfirst(from->SourceName()) + " earned the achievement '" + std::string(this->map->world->achievements_config["ReachLevel.Achievement." + util::to_string(from->level)]) + "'.");

                            from->NewAchievement(std::string(this->map->world->achievements_config["ReachLevel.Achievement." + util::to_string(from->level)]));
                        }
                    }
				}
			}

			if (spell_id != -1)
				builder.AddShort(spell_id);

            if (npckill)
            {
                builder.AddShort(0);
                builder.AddChar(from->direction);
            }
            else
            {
                builder.AddShort(drop_winner ? drop_winner->player->id : from->player->id);
                builder.AddChar(drop_winner ? drop_winner->direction : from->direction);
            }

            builder.AddShort(this->index);
            builder.AddShort(dropuid);
            builder.AddShort(dropid);
            builder.AddChar(this->x);
            builder.AddChar(this->y);
            builder.AddInt(dropamount);
            builder.AddThree(amount);

			if (spell_id != -1)
				builder.AddShort(from->tp);

			if ((sharemode == 0 && character == from) || (sharemode != 0 && findopp != this->damagelist.end()))
			{
				builder.AddInt(character->exp);
			}

			if (from->fishing_exp)
            {
                from->fishing_exp = false;

                PacketBuilder reply(PACKET_RECOVER, PACKET_REPLY);
                reply.AddInt(character->exp);
                reply.AddShort(character->karma);
                reply.AddChar(character->level);
                character->Send(reply);
            }

            if (level_up)
            {
                builder.AddChar(character->level);
                builder.AddShort(character->statpoints);
                builder.AddShort(character->flevel);
                builder.AddShort(character->maxhp);
                builder.AddShort(character->maxtp);
                builder.AddShort(character->maxsp);

                PacketBuilder reply(PACKET_ITEM, PACKET_ACCEPT);
                reply.AddShort(character->player->id);

                UTIL_FOREACH(character->map->characters, checkchar)
                {
                    if (checkchar != character && checkchar->InRange(character))
                        checkchar->Send(reply);
                }
            }

            if (from->mining_exp)
            {
                from->mining_exp = false;

                PacketBuilder reply(PACKET_RECOVER, PACKET_REPLY);
                reply.AddInt(character->exp);
                reply.AddShort(character->karma);
                reply.AddChar(character->level);
                character->Send(reply);
            }

            if (level_up)
            {
                builder.AddChar(character->level);
                builder.AddShort(character->statpoints);
                builder.AddShort(character->mlevel);
                builder.AddShort(character->maxhp);
                builder.AddShort(character->maxtp);
                builder.AddShort(character->maxsp);

                PacketBuilder reply(PACKET_ITEM, PACKET_ACCEPT);
                reply.AddShort(character->player->id);

                UTIL_FOREACH(character->map->characters, checkchar)
                {
                    if (checkchar != character && checkchar->InRange(character))
                        checkchar->Send(reply);
                }
            }

            if (from->woodcutting_exp)
            {
                from->woodcutting_exp = false;

                PacketBuilder reply(PACKET_RECOVER, PACKET_REPLY);
                reply.AddInt(character->exp);
                reply.AddShort(character->karma);
                reply.AddChar(character->level);
                character->Send(reply);
            }

            if (level_up)
            {
                builder.AddChar(character->level);
                builder.AddShort(character->statpoints);
                builder.AddShort(character->wlevel);
                builder.AddShort(character->maxhp);
                builder.AddShort(character->maxtp);
                builder.AddShort(character->maxsp);

                PacketBuilder reply(PACKET_ITEM, PACKET_ACCEPT);
                reply.AddShort(character->player->id);

                UTIL_FOREACH(character->map->characters, checkchar)
                {
                    if (checkchar != character && checkchar->InRange(character))
                        checkchar->Send(reply);
                }
            }

            if (from->cooking_exp)
            {
                from->cooking_exp = false;

                PacketBuilder reply(PACKET_RECOVER, PACKET_REPLY);
                reply.AddInt(character->exp);
                reply.AddShort(character->karma);
                reply.AddChar(character->level);
                character->Send(reply);
            }

            if (level_up)
            {
                builder.AddChar(character->level);
                builder.AddShort(character->statpoints);
                builder.AddShort(character->clevel);
                builder.AddShort(character->maxhp);
                builder.AddShort(character->maxtp);
                builder.AddShort(character->maxsp);

                PacketBuilder reply(PACKET_ITEM, PACKET_ACCEPT);
                reply.AddShort(character->player->id);

                UTIL_FOREACH(character->map->characters, checkchar)
                {
                    if (checkchar != character && checkchar->InRange(character))
                        checkchar->Send(reply);
                }
            }

			if (level_up)
			{
				builder.AddChar(character->level);
				builder.AddShort(character->statpoints);
				builder.AddShort(character->skillpoints);
				builder.AddShort(character->maxhp);
				builder.AddShort(character->maxtp);
				builder.AddShort(character->maxsp);
			}

			character->Send(builder);
		}
	}

	UTIL_FOREACH(parties, party)
	{
		party->ShareEXP(party->temp_expsum, partysharemode, this->map);
		party->temp_expsum = 0;
	}

	UTIL_FOREACH(this->damagelist, opponent)
	{
		opponent->attacker->unregister_npc.erase(std::remove(UTIL_RANGE(opponent->attacker->unregister_npc), this), opponent->attacker->unregister_npc.end());
	}

	this->damagelist.clear();
	this->totaldamage = 0;

	short childid = -1;

	if (this->Data().boss)
	{
		UTIL_FOREACH(this->map->npcs, npc)
		{
			if (npc->Data().child && !npc->Data().boss)
			{
				if (!npc->temporary && (childid == -1 || childid == npc->Data().id))
				{
					npc->Die(false);
					childid = npc->Data().id;
				}
				else
				{
					npc->Die(true);
				}
			}
		}
	}

	if (childid != -1)
	{
		PacketBuilder builder(PACKET_NPC, PACKET_JUNK, 2);
		builder.AddShort(childid);

		UTIL_FOREACH(this->map->characters, character)
		{
			character->Send(builder);
		}
	}

	if (this->temporary)
	{
		this->map->npcs.erase(std::remove(this->map->npcs.begin(), this->map->npcs.end(), this), this->map->npcs.end());
	}

    if (from->party)
    {
        if (std::string(from->world->config["KillNPCPartyShare"]) == "yes")
        {
            UTIL_FOREACH(from->party->members, member)
            {
                if (member->mapid == from->mapid)
                {
                    UTIL_FOREACH(member->quests, q)
                    {
                        if (!q.second || q.second->GetQuest()->Disabled())
                			continue;

                		q.second->KilledNPC(this->Data().id);
                    }
                }
            }

            if (this->temporary)
            {
                delete this;
                return;
            }
        }
    }
    else
    {
        UTIL_FOREACH(from->quests, q)
        {
            if (!q.second || q.second->GetQuest()->Disabled())
                continue;

            q.second->KilledNPC(this->Data().id);
        }

        if (this->temporary)
        {
            delete this;
            return;
        }
    }
}

void NPC::Die(bool show)
{
    if (!this->alive)
		return;

	this->alive = false;
	this->parent = 0;
	this->dead_since = int(Timer::GetTime());

	UTIL_FOREACH(this->damagelist, opponent)
	{
		opponent->attacker->unregister_npc.erase(std::remove(UTIL_RANGE(opponent->attacker->unregister_npc), this),opponent->attacker->unregister_npc.end());
	}

	this->damagelist.clear();
	this->totaldamage = 0;

	if (show)
	{
		PacketBuilder builder(PACKET_NPC, PACKET_SPEC, 18);
		builder.AddShort(0);
		builder.AddChar(0);
		builder.AddShort(this->index);
		builder.AddShort(0);
		builder.AddShort(0);
		builder.AddChar(this->x);
		builder.AddChar(this->y);
		builder.AddInt(0);
		builder.AddThree(this->hp);

		UTIL_FOREACH(this->map->characters, character)
		{
			if (character->InRange(this))
				character->Send(builder);
		}
	}

	if (this->temporary)
    {
        this->map->npcs.erase(std::remove(this->map->npcs.begin(), this->map->npcs.end(), this), this->map->npcs.end());

        delete this;
    }
}

// NPC's attacking players.

void NPC::Attack(Character *target)
{
    for (int i = 0 ; i < int(this->map->world->effects_config["NAmount"]) ; i++)
    {
        int npcid = this->map->world->effects_config[util::to_string(i+1) + ".NpcID"];
        int effect = this->map->world->effects_config[util::to_string(i+1) + ".Effect"];

        if (this->id == npcid)
        {
            target->Effect(effect, true);
        }
    }

    int amount = 0;

    if (this->pet && this->Data().type == ENF::Quest)
    {
        amount = util::rand(this->Data().mindam + this->mindam, this->Data().maxdam + static_cast<int>(this->map->world->config["NPCAdjustMaxDam"]) + this->maxdam);
    }
    else
    {
        amount = util::rand(this->Data().mindam, this->Data().maxdam + static_cast<int>(this->map->world->config["NPCAdjustMaxDam"]));
    }

    double rand = util::rand(0.0, 1.0);
    bool critical = std::abs(int(target->direction) - this->direction) != 2 || rand < static_cast<double>(this->map->world->config["CriticalRate"]);

    std::unordered_map<std::string, double> formula_vars;

    this->FormulaVars(formula_vars);
    target->FormulaVars(formula_vars, "target_");
    formula_vars["modifier"] = 1.0 / static_cast<double>(this->map->world->config["MobRate"]);
    formula_vars["damage"] = amount;
    formula_vars["critical"] = critical;

    amount = rpn_eval(rpn_parse(this->map->world->formulas_config["damage"]), formula_vars);
    double hit_rate = rpn_eval(rpn_parse(this->map->world->formulas_config["hit_rate"]), formula_vars);

    if (this->map->world->config["AntiGank"])
    {
        if (target->HasPet && this->map->pk)
        {
            if (this->pet)
            {
                int PetAntiGank(int minlevel,int maxlevel);
                int diff = int(this->map->world->config["AntiGankLevel"]);

                if (PetAntiGank(target->level, this->owner->level) >= diff)
                {
                    if (Timer::GetTime() > this->warntimer)
                    {
                        this->owner->StatusMsg("Pet cannot attack target - Not within level range [" + util::to_string(int(this->map->world->config["AntiGankLevel"])) + "]");
                        this->warntimer = Timer::GetTime() + 10;
                    }

                    amount = 0;
                }
            }
        }
    }

    if (rand > hit_rate)
        amount = 0;

    amount = std::max(amount, 0);

    int limitamount = std::min(amount, int(target->hp));

    if (this->map->world->config["LimitDamage"])
        amount = limitamount;

    if (target->immune == false)
        target->hp -= limitamount;

    if (target->party)
        target->party->UpdateHP(target);

    int xdiff = this->x - target->x;
    int ydiff = this->y - target->y;

    if (std::abs(xdiff) > std::abs(ydiff))
    {
        if (xdiff < 0)
        {
            this->direction = DIRECTION_RIGHT;
        }
        else
        {
            this->direction = DIRECTION_LEFT;
        }
    }
    else
    {
        if (ydiff < 0)
        {
            this->direction = DIRECTION_DOWN;
        }
        else
        {
            this->direction = DIRECTION_UP;
        }
    }

    PacketBuilder builder(PACKET_NPC, PACKET_PLAYER, 18);
    builder.AddByte(255);
    builder.AddChar(this->index);
    builder.AddChar(1 + (target->hp == 0));
    builder.AddChar(this->direction);
    builder.AddShort(target->player->id);
    builder.AddThree(target->immune == false ? amount : 0);
    builder.AddThree(util::clamp<int>(double(target->hp) / double(target->maxhp) * 100.0, 0, 100));
    builder.AddByte(255);
    builder.AddByte(255);

    UTIL_FOREACH(this->map->characters, character)
    {
        if (character == target || !character->InRange(target))
            continue;

        character->Send(builder);
    }

    int rechp = int(target->maxhp * static_cast<double>(this->map->world->config["DeathRecover"]));

    if (target->hp == 0)
        builder.AddShort(rechp);
    else
        builder.AddShort(target->hp);

    builder.AddShort(target->tp);
    target->Send(builder);

    if (target->hp == 0)
        target->DeathRespawn();
}

void NPC::ShowDialog(std::string message)
{
    if (message != "0")
    {
        PacketBuilder(builder);
        builder.SetID(PACKET_NPC, PACKET_PLAYER);
        builder.AddByte(255);
        builder.AddByte(255);
        builder.AddChar(this->index);
        builder.AddChar(message.length());
        builder.AddString(message);

        UTIL_FOREACH(this->map->characters, character)
        {
            if (character->InRange(this))
            {
                character->player->client->Send(builder);
            }
        }
    }
}

#define v(x) vars[prefix + #x] = x;
#define vv(x, n) vars[prefix + n] = x;
#define vd(x) vars[prefix + #x] = data.x;

void NPC::FormulaVars(std::unordered_map<std::string, double> &vars, std::string prefix)
{
	const ENF_Data& data = this->Data();
	vv(1, "npc") v(hp) vv(data.hp, "maxhp")
	vd(mindam) vd(maxdam)
	vd(accuracy) vd(evade) vd(armor)
	v(x) v(y) v(direction) vv(map->id, "mapid")
}

#undef vd
#undef vv
#undef v

NPC::~NPC()
{
	UTIL_FOREACH(this->map->characters, character)
	{
		if (character->npc == this)
		{
			character->npc = 0;
			character->npc_type = ENF::NPC;
		}
	}
}
