#include "handlers.hpp"

#include <algorithm>
#include <functional>

#include "../character.hpp"
#include "../eodata.hpp"
#include "../map.hpp"
#include "../player.hpp"
#include "../npc.hpp"
#include "../party.hpp"
#include "../util/rpn.hpp"

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

namespace Handlers
{
    void Spell_Request(Character *character, PacketReader &reader)
    {
        std::list<int> except_list = ExceptUnserialize(character->world->config["SpellMapExceptions"]);

        unsigned short spellid = reader.GetShort();
        int timestamp = reader.GetThree();

        character->timestamp = timestamp;

        character->CancelSpell();

        if (character->HasSpell(spellid))
        {
            if (std::find(except_list.begin(), except_list.end(), character->mapid) != except_list.end())
            {
                character->ServerMsg("Spells are not allowed here!");
                return;
            }
            else
            {
                const ESF_Data& spell = character->world->esf->Get(spellid);

                if (spell.id == 0)
                    return;

                if (spellid == int(character->world->pets_config["PetInventorySpellID"]))
                {
                    if (character->HasPet)
                        character->pet->OpenInventory();
                }

                for (int i = 0; i < int(character->world->buffspells_config["Amount"]); i++)
                {
                    if (spellid == int(character->world->buffspells_config[util::to_string(i+1) + ".BuffSpell"]))
                    {
                        int effect = character->world->buffspells_config[util::to_string(i+1) + ".BuffEffect"];
                        int str = character->world->buffspells_config[util::to_string(i+1) + ".BuffSTR"];
                        int lnt = character->world->buffspells_config[util::to_string(i+1) + ".BuffINT"];
                        int wis = character->world->buffspells_config[util::to_string(i+1) + ".BuffWIS"];
                        int agi = character->world->buffspells_config[util::to_string(i+1) + ".BuffAGI"];
                        int con = character->world->buffspells_config[util::to_string(i+1) + ".BuffCON"];
                        int cha = character->world->buffspells_config[util::to_string(i+1) + ".BuffCHA"];
                        int time = character->world->buffspells_config[util::to_string(i+1) + ".BuffTime"];
                        double exp = character->world->buffspells_config[util::to_string(i+1) + ".EXPRate"];
                        double drop = character->world->buffspells_config[util::to_string(i+1) + ".DropRate"];

                        if (spell.target == ESF::Self)
                        {
                            if (character->boosttimer < Timer::GetTime())
                            {
                                character->boosttimer = Timer::GetTime() + int(time);

                                if (effect > 0) character->boosteffect = effect;
                                if (str > 0) character->booststr += str;
                                if (lnt > 0) character->boostint += lnt;
                                if (wis > 0) character->boostwis += wis;
                                if (agi > 0) character->boostagi += agi;
                                if (con > 0) character->boostcon += con;
                                if (cha > 0) character->boostcha += cha;
                                if (exp > 0) character->boostexp = exp;
                                if (drop > 0) character->boostdrop = drop;

                                character->CalculateStats();
                                character->PacketRecover();
                                character->UpdateStats();

                                character->ServerMsg("You have been boosted for " + util::to_string(time) + " seconds.");

                                if (character->world->buffspells_config[util::to_string(i+1) + ".DeleteSpell"])
                                {
                                    character->DelSpell(spellid);

                                    PacketBuilder reply(PACKET_STATSKILL, PACKET_REMOVE, 2);
                                    reply.AddShort(spellid);
                                    character->Send(reply);
                                }
                            }
                            else
                            {
                                character->ServerMsg("you are already boosted!");
                            }
                        }
                        else if (spell.target == ESF::Group)
                        {
                            if (character->party)
                            {
                                UTIL_FOREACH(character->party->members, member)
                                {
                                    if (member->boosttimer < Timer::GetTime())
                                    {
                                        member->boosttimer = Timer::GetTime() + int(time);

                                        if (effect > 0) member->boosteffect = effect;
                                        if (str > 0) member->booststr += str;
                                        if (lnt > 0) member->boostint += lnt;
                                        if (wis > 0) member->boostwis += wis;
                                        if (agi > 0) member->boostagi += agi;
                                        if (con > 0) member->boostcon += con;
                                        if (cha > 0) member->boostcha += cha;
                                        if (exp > 0) member->boostexp = exp;
                                        if (drop > 0) member->boostdrop = drop;

                                        member->CalculateStats();
                                        member->PacketRecover();
                                        member->UpdateStats();

                                        member->ServerMsg(util::ucfirst(character->SourceName()) + " has boosted the party for " + util::to_string(time) + " seconds.");

                                        if (character->world->buffspells_config[util::to_string(i+1) + ".DeleteSpell"])
                                        {
                                            character->DelSpell(spellid);

                                            PacketBuilder reply(PACKET_STATSKILL, PACKET_REMOVE, 2);
                                            reply.AddShort(spellid);
                                            character->Send(reply);
                                        }
                                    }
                                    else
                                    {
                                        member->ServerMsg(util::ucfirst(character->SourceName()) + " boosted the party, but you are already boosted so this won't affect you.");
                                    }
                                }
                            }
                        }
                    }
                }

                if (spellid == int(character->world->buffspells_config["UndoBuffSpell"]))
                    character->UndoBuff();

                for (int i = 0; i < int(character->world->spells_config["HideSpellAmount"]); i++)
                {
                    if (spellid == int(character->world->spells_config[util::to_string(i+1) + ".HideSpell"]))
                    {
                        if (character->hidetimer < Timer::GetTime())
                        {
                            character->hidetimer = Timer::GetTime() + int(character->world->spells_config[util::to_string(i+1) + ".HideTimer"]);

                            if (character->admin == ADMIN_PLAYER)
                            {
                                if (!character->hidden)
                                {
                                    character->Hide();
                                    character->Refresh();
                                }
                            }

                            character->ServerMsg("You are now hidden for " + util::to_string(int(character->world->spells_config[util::to_string(i+1) + ".HideTimer"])) + " seconds.");
                        }
                        else
                        {
                            character->ServerMsg("you are already hidden!");
                        }
                    }
                }

                if (character->world->esf->Get(spellid).unkp > 0 && character->world->config["WarpSpells"])
                {
                    if (character->party)
                    {
                        if (character == character->party->leader)
                        {
                            UTIL_FOREACH(character->party->members, member)
                            {
                                int M = util::to_int(util::explode(',', character->world->event_config["EventLocation"])[0]);

                                if (character->mapid == M && M > 0)
                                    return;

                                if (character->mapid != int(character->world->config["JailMap"]) && character->mapid != int(character->world->config["WallMap"])
                                    && character->mapid != int(character->world->devilgate_config["DevilMap"]) && character->mapid != int(character->world->ctf_config["CTFMap"])
                                    && character->mapid != util::to_int(util::explode(',', character->world->event_config["EventLocation"])[0]))
                                {
                                    if (member->mapid != int(character->world->config["JailMap"]) && member->mapid != int(character->world->config["WallMap"])
                                        && member->mapid != int(character->world->devilgate_config["DevilMap"]) && member->mapid != int(character->world->ctf_config["CTFMap"])
                                        && member->mapid != util::to_int(util::explode(',', character->world->event_config["EventLocation"])[0]))
                                    {
                                        if (member->SourceName() != character->SourceName())
                                            member->Warp(character->mapid, character->x, character->y, character->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                                    }
                                }
                                else
                                {
                                    character->ServerMsg("You may not cast that here!");
                                }
                            }
                        }
                        else
                        {
                            character->ServerMsg("You aren't the party leader!");
                        }
                    }
                    else
                    {
                        character->ServerMsg("You don't have a party!");
                    }
                }

                if (character->world->esf->Get(spellid).unkn > 0 && character->world->config["WarpSpells"])
                {
                    Character *victim = character->world->GetCharacter(character->partner);

                    if (character->partner == "")
                    {
                        character->ServerMsg("You don't have a partner!");
                        return;
                    }
                    else if (!victim || victim->nowhere)
                    {
                        character->ServerMsg("You partner is not online!");
                        return;
                    }
                    else
                    {
                        if (character->mapid != int(character->world->config["JailMap"]) && character->mapid != int(character->world->config["WallMap"]))
                        {
                            if (victim->mapid != int(character->world->config["JailMap"]) && victim->mapid != int(character->world->config["WallMap"])
                                && victim->mapid != int(character->world->devilgate_config["DevilMap"]) && victim->mapid != int(character->world->ctf_config["CTFMap"])
                                && victim->mapid != util::to_int(util::explode(',', character->world->event_config["EventLocation"])[0]))
                            {
                                if (victim->SourceName() != character->SourceName())
                                    character->Warp(victim->mapid, victim->x, victim->y, character->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
                            }
                        }
                    }
                }

                for (int i = 0 ; i < int(character->world->pets_config["PetAmount"]); i++)
                {
                    int spell = character->world->pets_config[util::to_string(i+1) + ".SpellID"];
                    int npcid = character->world->pets_config[util::to_string(i+1) + ".NpcID"];

                    std::list<int> except_list = ExceptUnserialize(character->world->pets_config["AntiSpawnIDs"]);

                    if (spellid == spell)
                    {
                        if (std::find(except_list.begin(), except_list.end(), character->mapid) != except_list.end())
                        {
                            character->StatusMsg("You cannot spawn any pets on this map!");
                        }
                        else
                        {
                            character->HatchPet(character, npcid);

                            if (!character->HasPet)
                            {
                                character->AddPet(character, npcid);
                            }
                            else
                            {
                                character->PetTransfer();
                                character->pettransfer = false;
                            }
                        }
                    }
                }

                character->spell_id = spellid;
                character->spell_event = new TimeEvent(character_cast_spell, character, 0.47 * spell.cast_time, 1);
                character->world->timer.Register(character->spell_event);

                PacketBuilder builder(PACKET_SPELL, PACKET_REQUEST, 4);
                builder.AddShort(character->player->id);
                builder.AddShort(spellid);

                UTIL_FOREACH(character->map->characters, updatecharacter)
                {
                    if (updatecharacter != character && character->InRange(updatecharacter))
                    {
                        updatecharacter->Send(builder);
                    }
                }
            }
        }
    }

    void Spell_Target_Self(Character *character, PacketReader &reader)
    {
        reader.GetChar();
        reader.GetShort();

        Timestamp timestamp = reader.GetThree();

        if (!character->spell_event && !character->spell_ready)
        {
            return;
        }

        character->spell_target = Character::TargetSelf;
        character->spell_target_id = 0;

        if (character->world->config["EnforceTimestamps"])
        {
            const ESF_Data& spell = character->world->esf->Get(character->spell_id);

            if (timestamp - character->timestamp < (spell.cast_time - 1) * 47 + 35
            || timestamp - character->timestamp > spell.cast_time * 50)
            {
                character->CancelSpell();
                return;
            }
            else
            {
                if (character->world->esf->Get(character->spell_id).unkc > 0 && character->world->config["AOESpells"])
                {
                    if (character->tp < spell.tp)
                        return;

                    int target_x = character->x;
                    int target_y = character->y;

                    switch (character->direction)
                    {
                        case DIRECTION_UP:
                        target_y -= 1;
                        break;

                        case DIRECTION_RIGHT:
                        target_x += 1;
                        break;

                        case DIRECTION_DOWN:
                        target_y += 1;
                        break;

                        case DIRECTION_LEFT:
                        target_x -= 1;
                        break;
                    }

                    if (spell.target_restrict != ESF::Friendly && spell.target_restrict != ESF::Opponent)
                        character->tp -= spell.tp;

                    UTIL_FOREACH(character->map->npcs, npc)
                    {
                        int amount = util::rand(character->mindam, character->maxdam) + util::rand(spell.mindam, spell.maxdam);
                        double rand = util::rand(0.0, 1.0);
                        bool critical = rand < static_cast<double>(character->world->config["CriticalRate"]);

                        std::unordered_map<std::string, double> formula_vars;

                        character->FormulaVars(formula_vars);
                        npc->FormulaVars(formula_vars, "target_");
                        formula_vars["modifier"] = character->world->config["MobRate"];
                        formula_vars["damage"] = amount;
                        formula_vars["critical"] = critical;

                        amount = rpn_eval(rpn_parse(character->world->formulas_config["damage"]), formula_vars);
                        double hit_rate = rpn_eval(rpn_parse(character->world->formulas_config["hit_rate"]), formula_vars);

                        if (rand > hit_rate)
                            amount = 0;

                        amount = std::max(amount, 0);

                        int limitamount = std::min(amount, int(npc->hp));

                        if (character->world->config["LimitDamage"])
                            amount = limitamount;

                        if (npc->Data().type == ENF::Passive || npc->Data().type == ENF::Aggressive || npc->pet)
                        {
                            int distance = util::path_length(target_x, target_y, npc->x, npc->y);

                            if ((character->map->pk == false && npc->pet) || character == npc->owner)
                                return;

                            if (distance <= character->world->esf->Get(character->spell_id).unkc && npc->alive)
                                npc->Damage(character, amount, character->spell_id);
                        }
                    }
                }
            }
        }

        character->timestamp = timestamp;

        if (character->spell_ready)
        {
            character->SpellAct();
        }
    }

    void Spell_Target_Other(Character *character, PacketReader &reader)
    {
        unsigned char target_type = reader.GetChar();

        reader.GetChar();
        reader.GetShort();
        reader.GetShort();

        unsigned short victim_id = reader.GetShort();

        Timestamp timestamp = reader.GetThree();

        if (!character->spell_event && !character->spell_ready)
        {
            return;
        }

        switch (target_type)
        {
            case 1:
            character->spell_target = Character::TargetPlayer;
            character->spell_target_id = victim_id;
            break;

            case 2:
            character->spell_target = Character::TargetNPC;
            character->spell_target_id = victim_id;
            break;

            default:
            character->CancelSpell();
            return;
        }

        if (character->world->config["EnforceTimestamps"])
        {
            const ESF_Data& spell = character->world->esf->Get(character->spell_id);

            if (timestamp - character->timestamp < (spell.cast_time - 1) * 47 + 35
             || timestamp - character->timestamp > spell.cast_time * 50)
            {
                character->CancelSpell();
                return;
            }
        }

        character->timestamp = timestamp;

        if (character->spell_ready)
        {
            character->SpellAct();
        }
    }

    void Spell_Target_Group(Character *character, PacketReader &reader)
    {
        reader.GetShort();

        Timestamp timestamp = reader.GetThree();

        if (!character->spell_event && !character->spell_ready)
        {
            return;
        }

        character->spell_target = Character::TargetGroup;
        character->spell_target_id = 0;

        if (character->world->config["EnforceTimestamps"])
        {
            const ESF_Data& spell = character->world->esf->Get(character->spell_id);

            if (timestamp - character->timestamp < (spell.cast_time - 1) * 47 + 35
             || timestamp - character->timestamp > spell.cast_time * 50)
            {
                character->CancelSpell();
                return;
            }
        }

        character->timestamp = timestamp;

        if (character->spell_ready)
        {
            character->SpellAct();
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_SPELL)
        Register(PACKET_REQUEST, Spell_Request, Playing);
        Register(PACKET_TARGET_SELF, Spell_Target_Self, Playing);
        Register(PACKET_TARGET_OTHER, Spell_Target_Other, Playing);
        Register(PACKET_TARGET_GROUP, Spell_Target_Group, Playing);
    PACKET_HANDLER_REGISTER_END()
}
