#include "handlers.hpp"

#include <deque>
#include <memory>

#include "../character.hpp"
#include "../dialog.hpp"
#include "../eoplus.hpp"
#include "../npc.hpp"
#include "../packet.hpp"
#include "../quest.hpp"

#include "../console.hpp"

namespace Handlers
{
    static void open_quest_dialog(Character* character, NPC* npc, int quest_id, int vendor_id)
    {
        struct dialog_t
        {
            int quest_id;
            const std::shared_ptr<Quest_Context> quest;
            const Dialog* dialog;
        };

        std::deque<dialog_t> dialogs;
        std::size_t this_dialog = 0;

        UTIL_FOREACH(character->quests, quest)
        {
            if (!quest.second || quest.second->GetQuest()->Disabled())
                continue;

            const Dialog* dialog = quest.second->GetDialog(vendor_id);

            if (dialog)
            {
                dialogs.push_back(dialog_t{quest.first, quest.second, dialog});

                if (quest.first == quest_id)
                    this_dialog = dialogs.size() - 1;
            }
        }

        if (!dialogs.empty())
        {
            character->npc = npc;
            character->npc_type = ENF::Quest;

            PacketBuilder reply(PACKET_QUEST, PACKET_DIALOG, 10);
            reply.AddChar(dialogs.size());
            reply.AddShort(vendor_id);
            reply.AddShort(dialogs[this_dialog].quest_id);
            reply.AddShort(0);
            reply.AddShort(0);
            reply.AddByte(255);

            std::size_t reserve = 0;

            UTIL_FOREACH(dialogs, dialog)
            {
                reserve += 3 + dialog.quest->GetQuest()->Name().length();
            }

            reply.ReserveMore(reserve);

            UTIL_FOREACH(dialogs, dialog)
            {
                reply.AddShort(dialog.quest_id);
                reply.AddBreakString(dialog.quest->GetQuest()->Name());
            }

            dialogs[this_dialog].dialog->BuildPacket(reply);

            character->Send(reply);
        }
    }

    void Quest_Use(Character *character, PacketReader &reader)
    {
        short npc_index = reader.GetShort();
        short quest_id = reader.GetShort();

        UTIL_FOREACH(character->map->npcs, npc)
        {
            if (npc->index == npc_index && npc->Data().type == ENF::Quest && character->InRange(npc))
            {
                short vendor_id = npc->Data().vendor_id;

                if (npc->pet && npc->owner)
                {
                    character->npc = npc;
                    character->npc_type = ENF::Quest;

                    PacketBuilder builder;
                    builder.SetID(PACKET_QUEST, PACKET_DIALOG);
                    builder.AddChar(1);
                    builder.AddShort(446); // Quest ID
                    builder.AddShort(446); // Quest ID
                    builder.AddInt(0);
                    builder.AddByte(255);
                    builder.AddShort(446); // Quest ID
                    builder.AddBreakString(util::ucfirst(npc->name) + "'s Information");
                    builder.AddShort(1);
                    builder.AddBreakString("Owner: " + util::ucfirst(npc->owner->SourceName()) + ", level: " + util::to_string(npc->level) + " (" + util::to_string(npc->rebirth) + "), experience: " + util::to_string(npc->exp) + "/" + util::to_string(character->world->exp_table[npc->level + 1]) + + ", health points: " + util::to_string(npc->hp) + "/" + util::to_string(npc->Data().hp) + ", technique points: " + util::to_string(npc->tp) + "/" + util::to_string(npc->maxtp) + ", damage: " + util::to_string(npc->Data().mindam + npc->mindam) + "-" + util::to_string(npc->Data().maxdam + int(character->world->config["NPCAdjustMaxDam"]) + npc->maxdam));

                    if (character->HasPet && npc->owner == character)
                    {
                        builder.AddShort(2); // Input type
                        builder.AddShort(3); // Input number
                        builder.AddBreakString("Open Inventory");
                        builder.AddShort(2); // Input type
                        builder.AddShort(4); // Input number
                        builder.AddBreakString("Despawn Pet");
                    }

                    character->Send(builder);
                }
                else
                {
                    if (vendor_id < 1)
                        continue;

                    if (quest_id == 0)
                        quest_id = vendor_id;

                    auto context = character->GetQuest(vendor_id);

                    if (!context)
                    {
                        auto it = character->world->quests.find(vendor_id);

                        if (it != character->world->quests.end())
                        {
                            Quest* quest = it->second.get();

                            if (quest->Disabled())
                                continue;

                            auto adj_context = std::make_shared<Quest_Context>(character, quest);
                            character->quests[it->first] = adj_context;
                            adj_context->SetState("begin");
                        }
                    }
                    else if (context->StateName() == "done")
                    {
                        context->SetState("begin");
                    }

                    open_quest_dialog(character, npc, quest_id, vendor_id);

                    break;
                }
            }
        }
    }

    void Quest_Accept(Character *character, PacketReader &reader)
    {
        reader.GetShort();
        reader.GetShort();

        short quest_id = reader.GetShort();
        reader.GetShort();
        DialogReply type = DialogReply(reader.GetChar());

        char action = 0;

        if (type == DIALOG_REPLY_LINK)
            action = reader.GetChar();

        if (quest_id == 400)
        {
            if (type == 2)
            {
                for (int i = 0; i < int(character->world->shrines_config["Amount"]); i++)
                {
                    int M = int(character->world->shrines_config[util::to_string(i+1) + ".WarpMap"]);
                    int X = int(character->world->shrines_config[util::to_string(i+1) + ".WarpX"]);
                    int Y = int(character->world->shrines_config[util::to_string(i+1) + ".WarpY"]);
                    int cost = int(character->world->shrines_config[util::to_string(i+1) + ".WarpCost"]);
                    int level = int(character->world->shrines_config[util::to_string(i+1) + ".WarpLevel"]);
                    int rebirth = int(character->world->shrines_config[util::to_string(i+1) + ".WarpRebirth"]);

                    std::string name = std::string(character->world->shrines_config[util::to_string(i+1) + ".WarpName"]);

                    if (action == (i+1))
                    {
                        if ((character->level < level && character->rebirth == 0) || (character->rebirth < rebirth))
                        {
                            if (character->level < level && character->rebirth == 0)
                                character->StatusMsg("You need level " + util::to_string(level) + " to warp to " + name);
                            else if (character->rebirth < rebirth)
                                character->StatusMsg("You need rebirth level " + util::to_string(rebirth) + " to warp to " + name);

                            return;
                        }

                        if (character->HasItem(1) >= cost)
                        {
                            character->Warp(M,X,Y);
                            character->RemoveItem(1, cost);
                            character->StatusMsg("You warped to " + util::ucfirst(name));
                        }
                        else
                        {
                            character->StatusMsg("You don't enough " + character->world->eif->Get(1).name);
                        }
                    }
                }
            }
        }

        if (quest_id == 445)
        {
            if (type == 2)
            {
                switch (action)
                {
                    case 3:
                        character->warn = 1;
                    break;

                    case 4:
                        character->warn = 2;
                    break;
                }
            }
		}

		if (quest_id == 446)
        {
            if (type == 2)
            {
                switch (action)
                {
                    case 3:
                        if (character->HasPet)
                            character->pet->OpenInventory();
                    break;

                    case 4:
                        if (character->HasPet)
                        {
                            character->PetTransfer();
                            character->pettransfer = false;
                        }
                    break;
                }
            }
		}

		if (quest_id == 447)
        {
            if (type == 2)
            {
                switch (action)
                {
                    case 3:
                        Character *victim;

                        victim = character->world->GetCharacter(character->transfer_target);

                        if (!victim->transfer_pending)
                        {
                            PacketBuilder builder;
                            builder.SetID(PACKET_QUEST, PACKET_DIALOG);
                            builder.AddChar(1);
                            builder.AddShort(448);
                            builder.AddShort(448);
                            builder.AddInt(0);
                            builder.AddByte(255);
                            builder.AddShort(448);
                            builder.AddBreakString("Pet transfer - " + character->transfer_petname);
                            builder.AddShort(1);
                            builder.AddBreakString(util::ucfirst(character->SourceName()) + " wants to sell his pet to you for " + character->transfer_cost + " " + character->world->eif->Get(1).name + ". Do you accept this transfer?");
                            builder.AddShort(2);
                            builder.AddShort(3);
                            builder.AddBreakString("Yes, accept request");
                            builder.AddShort(2);
                            builder.AddShort(4);
                            builder.AddBreakString("No, don't accept request");
                            victim->Send(builder);

                            if (victim->transfer_timer < Timer::GetTime())
                            {
                                victim->transfer_timer = Timer::GetTime() + 60.0;
                                    victim->transfer_pending = true;
                            }

                            victim->transfer_requester = character->SourceName();

                            character->ServerMsg("Waiting for " + util::ucfirst(victim->SourceName()) + " to accept or decline the transfer.");
                        }
                        else
                        {
                            victim->ServerMsg(util::ucfirst(character->SourceName()) + " already has a transfer request pending right now.");
                        }
                    break;

                    case 4:
                        if (character->transfer_pending)
                            character->ResetTransfer();
                    break;
                }
            }
		}

		if (quest_id == 448)
        {
            if (type == 2)
            {
                switch (action)
                {
                    case 3:
                        try
                        {
                            Character *victim;

                            if (character->transfer_requester != "")
                                victim = character->world->GetCharacter(character->transfer_requester);

                            if (!victim || !victim->online || victim->nowhere)
                                return;

                            if (!character->transfer_pending && character->transfer_requester != "")
                            {
                                character->transfer_pending = true;

                                if (character->HasItem(1) >= atoi(victim->transfer_cost.c_str()))
                                {
                                    if (victim->HasPet && victim->pet->id == victim->transfer_petid)
                                    {
                                        Database_Result res = victim->world->db.Query("SELECT `npcid` FROM `pets` WHERE `owner_name` = '$' AND `npcid` = #", character->SourceName().c_str(), victim->pet->id);

                                        if (res.empty())
                                        {
                                            character->world->db.Query("DELETE FROM `pets` WHERE npcid = '#'", victim->pet->id);

                                            victim->PetTransfer();
                                            victim->pettransfer = false;
                                            victim->ServerMsg("You successfully sold your pet to " + util::ucfirst(character->SourceName()) + " for " + victim->transfer_cost + " " + character->world->eif->Get(1).name);

                                            character->RemoveItem(1, atoi(victim->transfer_cost.c_str()));
                                            character->HatchPet(character, victim->pet->id);
                                            character->ServerMsg(util::ucfirst(victim->SourceName()) + " has sold you his pet.");

                                            victim->GiveItem(1, atoi(victim->transfer_cost.c_str()));

                                            victim->ResetTransfer();
                                            character->ResetTransfer();
                                        }
                                        else
                                        {
                                            victim->ServerMsg(util::ucfirst(character->SourceName()) + " already has this pet.");
                                                victim->ResetTransfer();

                                            character->ServerMsg("You already have this pet.");
                                                character->ResetTransfer();
                                        }
                                        break;
                                    }
                                    else
                                    {
                                        victim->ServerMsg("pet ID: " + util::to_string(victim->pet->id) + " transfer id: " + util::to_string(victim->transfer_petid));

                                        character->ServerMsg("The transfer did not complete because " + util::ucfirst(victim->SourceName()) + " no longer has the correct pet spawned.");
                                            character->ResetTransfer();

                                        victim->ServerMsg("The transfer did not complete because you no longer have your pet spawned.");
                                            victim->ResetTransfer();
                                    }
                                }
                                else
                                {
                                    character->ServerMsg("You do not have enough money for the transfer.");
                                        character->ResetTransfer();

                                    victim->ServerMsg(util::ucfirst(character->SourceName()) + " does not have enough money for the transfer.");
                                        victim->ResetTransfer();
                                }
                            }
                            else
                            {
                                victim->ServerMsg(util::ucfirst(character->SourceName()) + " already has a transfer request pending right now.");
                                    victim->ResetTransfer();
                            }
                        }
                        catch (std::exception& e)
                        {

                        }
                    break;

                    case 4:
                        try
                        {
                            Character *requester;

                            if (character->transfer_requester != "")
                                requester = character->world->GetCharacter(character->transfer_requester);

                            if (!requester || !requester->online || requester->nowhere)
                                return;

                            if (character->transfer_requester != "")
                            {
                                requester->ServerMsg(util::ucfirst(character->SourceName()) + " has declined the transfer request.");
                                    requester->ResetTransfer();

                                character->ResetTransfer();
                            }
                        }
                        catch (std::exception& e)
                        {

                        }
                    break;
                }
            }
		}

        if (character->npc_type == ENF::Quest)
        {
            short vendor_id = character->npc->Data().vendor_id;
            std::shared_ptr<Quest_Context> quest;

            auto it = character->quests.find(quest_id);

            if (it != character->quests.end())
            {
                const Dialog* dialog = it->second->GetDialog(vendor_id);

                if (dialog && (type == DIALOG_REPLY_OK || (type == DIALOG_REPLY_LINK && dialog->CheckLink(action))))
                {
                    quest = it->second;
                }
            }

            if (quest && !quest->GetQuest()->Disabled())
            {
                bool result = action ? quest->DialogInput(action) : quest->TalkedNPC(vendor_id);

                quest = character->GetQuest(quest_id);

                if (result && quest && !quest->GetQuest()->Disabled() && !quest->Finished() && character->npc)
                {
                    open_quest_dialog(character, character->npc, quest_id, vendor_id);
                }
            }
        }
    }

    void Quest_List(Character *character, PacketReader &reader)
    {
        QuestPage page = QuestPage(reader.GetChar());

        PacketBuilder reply(PACKET_QUEST, PACKET_LIST, 4);
        reply.AddChar(page);
        reply.AddShort(character->quests.size());

        std::size_t reserve = 0;

        switch (page)
        {
            case QUEST_PAGE_PROGRESS:
            UTIL_FOREACH(character->quests, q)
            {
                if (q.second && !q.second->Finished() && !q.second->GetQuest()->Disabled() && !q.second->IsHidden())
                    reserve += 9 + q.second->GetQuest()->GetQuest()->info.name.length() + q.second->Desc().length();
            }

            reply.ReserveMore(reserve);

            UTIL_FOREACH(character->quests, q)
            {
                if (!q.second || q.second->Finished() || q.second->GetQuest()->Disabled() || q.second->IsHidden())
                    continue;

                Quest_Context::ProgressInfo progress = q.second->Progress();

                reply.AddBreakString(q.second->GetQuest()->GetQuest()->info.name);
                reply.AddBreakString(q.second->Desc());
                reply.AddShort(progress.icon);
                reply.AddShort(progress.progress);
                reply.AddShort(progress.target);
                reply.AddByte(255);
            }
            break;

            case QUEST_PAGE_HISTORY:
            UTIL_FOREACH(character->quests, q)
            {
                if (q.second && q.second->Finished() && !q.second->IsHidden())
                    reserve += 1 + q.second->GetQuest()->GetQuest()->info.name.length();
            }

            UTIL_FOREACH(character->achievements, achievement)
            {
                reply.AddBreakString(achievement.name);
            }

            reply.ReserveMore(reserve);

            UTIL_FOREACH(character->quests, q)
            {
                if (q.second && q.second->Finished() && !q.second->IsHidden())
                    reply.AddBreakString(q.second->GetQuest()->GetQuest()->info.name);
            }
            break;
        }

        character->Send(reply);
    }

    PACKET_HANDLER_REGISTER(PACKET_QUEST)
        Register(PACKET_USE, Quest_Use, Playing);
        Register(PACKET_ACCEPT, Quest_Accept, Playing);
        Register(PACKET_LIST, Quest_List, Playing);
    PACKET_HANDLER_REGISTER_END()
}
