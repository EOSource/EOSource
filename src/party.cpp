#include "party.hpp"

#include <cmath>

#include "character.hpp"
#include "eoclient.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "world.hpp"

Party::Party(World *world, Character *leader, Character *other)
{
	this->world = world;
	this->world->parties.push_back(this);

	this->members.push_back(leader);
	this->members.push_back(other);

	this->leader = leader;

	leader->party = this;
	other->party = this;

	this->temp_expsum = 0;

	this->RefreshMembers(leader, true);
	this->RefreshMembers(other, true);
}

void Party::Msg(Character *from, std::string message, bool echo)
{
	message = util::text_cap(message, static_cast<int>(this->world->config["ChatMaxWidth"]) - util::text_width(util::ucfirst(from->SourceName()) + "  "));

	PacketBuilder builder(PACKET_TALK, PACKET_OPEN, 2 + message.length());

	builder.AddShort(from->player->id);
	builder.AddString(message);

	UTIL_FOREACH(this->members, member)
	{
        member->AddChatLog("'", from->SourceName(), message);

		if (!echo && member == from)
			continue;

		member->Send(builder);
	}
}

void Party::Join(Character *character)
{
	character->party = this;

	this->members.push_back(character);

	PacketBuilder builder(PACKET_PARTY, PACKET_ADD, 5 + character->SourceName().length());
	builder.AddShort(character->player->id);
	builder.AddChar(character == this->leader);
	builder.AddChar(character->level);
	builder.AddChar(util::clamp<int>(double(character->hp) / double(character->maxhp) * 100.0, 0, 100));
	builder.AddString(character->SourceName());

	UTIL_FOREACH(this->members, checkcharacter)
	{
		if (checkcharacter != character)
		{
			checkcharacter->Send(builder);
		}
	}

	this->JoinMembers(character);
}

void Party::Leave(Character *character)
{
    if (character->mapid == int(this->world->partymap_config[util::to_string(character->mapid) + ".map"]))
    {
        world->PartyMaps();

        if (character->party)
        {
            UTIL_FOREACH(character->party->members, member)
            {
                if (character->party->members.size() <= int64_t(int(this->world->partymap_config[util::to_string(character->mapid) + ".partyreq"])))
                    member->ServerMsg("Someone left the party, you have " + util::to_string(double(this->world->partymap_config["KickTimer"])) + " seconds to re-group.");
                else
                    member->ServerMsg("Someone left the party.");
            }
        }
    }

	if (this->members.size() > 2 && character != this->leader)
	{
		UTIL_IFOREACH(this->members, checkcharacter)
		{
			if (*checkcharacter == character)
			{
				this->members.erase(checkcharacter);
				break;
			}
		}

		character->party = 0;

		PacketBuilder builder(PACKET_PARTY, PACKET_REMOVE, 2);
		builder.AddShort(character->player->id);
		UTIL_FOREACH(this->members, checkcharacter)
		{
			if (character != checkcharacter)
			{
				checkcharacter->Send(builder);
			}
		}

		builder.Reset(2);
		builder.SetID(PACKET_PARTY, PACKET_CLOSE);

		builder.AddShort(255);

		character->Send(builder);
	}
	else
	{
		delete this;
	}
}

void Party::JoinMembers(Character *character)
{
	PacketBuilder builder(PACKET_PARTY, PACKET_CREATE, this->members.size() * 18);

	UTIL_FOREACH(this->members, member)
	{
		builder.AddShort(member->player->id);
		builder.AddChar(member == this->leader);
		builder.AddChar(member->level);
		builder.AddChar(util::clamp<int>(double(member->hp) / double(member->maxhp) * 100.0, 0, 100));
		builder.AddBreakString(member->SourceName());
	}

	character->Send(builder);
}

void Party::RefreshMembers(Character *character, bool create)
{
	PacketBuilder builder(PACKET_PARTY, create ? PACKET_CREATE : PACKET_LIST, this->members.size() * 18);

	UTIL_FOREACH(this->members, member)
	{
		builder.AddShort(member->player->id);
		builder.AddChar(member == this->leader);
		builder.AddChar(member->level);
		builder.AddChar(util::clamp<int>(double(member->hp) / double(member->maxhp) * 100.0, 0, 100));
		builder.AddBreakString(member->SourceName());
	}

	character->Send(builder);
}

void Party::UpdateHP(Character *character)
{
	PacketBuilder builder(PACKET_PARTY, PACKET_AGREE, 3);
	builder.AddShort(character->player->id);
	builder.AddChar(util::clamp<int>(double(character->hp) / double(character->maxhp) * 100.0, 0, 100));

	UTIL_FOREACH(this->members, member)
	{
		member->Send(builder);
	}
}

void Party::ShareEXP(int exp, int sharemode, Map *map)
{
	int reward = 0;
	double sumlevel = 0;
	double members = 0;

	UTIL_FOREACH(this->members, member)
	{
	    if (!member->CanInteractCombat())
            continue;

		if (member->map == map && !member->nowhere)
		{
			if (member->level == 0)
			{
				++sumlevel;
			}
			else
			{
				sumlevel += member->level;
			}

			++members;
		}
	}

	if (members == 0 || sumlevel == 0)
        return;

	UTIL_FOREACH(this->members, member)
	{
	    if (!member->CanInteractCombat())
            continue;

		if (member->map != map || member->nowhere)
		{
			continue;
		}

		switch (sharemode)
		{
			case 1:
            reward = int(std::ceil(double(exp) / members));
            break;

			case 2:
            reward = int(std::ceil(double(exp) * double((member->level == 0) ? 1 : member->level) / sumlevel));
            break;
		}

		if (member->member == 0 && member->boostexp == 0)
        {
            member->GiveEXP(std::min(std::max(reward, 0), int(member->world->config["MaxExp"])));
        }
        else
        {
            if (member->member == 1)
                member->GiveEXP(std::min(std::max(int(reward + (reward * double(member->world->members_config["MemberEXPAmountL1"]))), 0), int(member->world->config["MaxExp"])));
            else if (member->member == 2)
                member->GiveEXP(std::min(std::max(int(reward + (reward * double(member->world->members_config["MemberEXPAmountL2"]))), 0), int(member->world->config["MaxExp"])));
            else if (member->member == 3)
                member->GiveEXP(std::min(std::max(int(reward + (reward * double(member->world->members_config["MemberEXPAmountL3"]))), 0), int(member->world->config["MaxExp"])));

            int boosted = int(std::ceil(double(reward) * member->boostexp));

            if (member->boosttimer > 0 && member->boostexp > 0)
                member->GiveEXP(std::min(std::max(int(boosted + int(boosted)), 0), int(member->world->config["MaxExp"])));
        }
	}
}

Party::~Party()
{
	if (this->world)
	{
		this->world->parties.erase(std::remove(this->world->parties.begin(), this->world->parties.end(), this),this->world->parties.end());
	}

	PacketBuilder builder(PACKET_PARTY, PACKET_CLOSE, 1);
	builder.AddByte(255);

	UTIL_FOREACH(this->members, member)
	{
		member->party = 0;
		member->Send(builder);
	}
}
