#include "handlers.hpp"

#include "../character.hpp"
#include "../npc.hpp"
#include "../eoserver.hpp"
#include "../map.hpp"
#include "../player.hpp"
#include "../world.hpp"

namespace Handlers
{
    static PacketBuilder add_common(Character *character, short item, int amount)
    {
        character->DelItem(item, amount);
        character->CalculateStats();

        PacketBuilder reply(PACKET_LOCKER, PACKET_REPLY, 8 + character->bank.size() * 5);
        reply.AddShort(item);
        reply.AddInt(character->HasItem(item));
        reply.AddChar(character->weight);
        reply.AddChar(character->maxweight);

        if (character->petinv_open)
        {
            UTIL_FOREACH(character->pet->inventory, item)
            {
                reply.AddShort(item.id);
                reply.AddThree(item.amount);
            }
        }
        else
        {
            UTIL_FOREACH(character->bank, item)
            {
                reply.AddShort(item.id);
                reply.AddThree(item.amount);
            }
        }

        return reply;
    }

    void Locker_Add(Character *character, PacketReader &reader)
    {
        if (character->trading) return;

        unsigned char x = reader.GetChar();
        unsigned char y = reader.GetChar();

        short item = reader.GetShort();
        int amount = reader.GetThree();

        if (item == 1) return;
        if (amount <= 0) return;
        if (character->HasItem(item) < amount) return;

        std::size_t lockermax = static_cast<int>(character->world->config["BaseBankSize"]) + character->bankmax * static_cast<int>(character->world->config["BankSizeStep"]);

        if (util::path_length(character->x, character->y, x, y) <= 1 && !character->petinv_open)
        {
            if (character->map->GetSpec(x, y) == Map_Tile::BankVault)
            {
                UTIL_IFOREACH(character->bank, it)
                {
                    if (it->id == item)
                    {
                        if (it->amount + amount < 0)
                        {
                            return;
                        }

                        amount = std::min<int>(amount, static_cast<int>(character->world->config["MaxBank"]) - it->amount);

                        it->amount += amount;

                        PacketBuilder reply = add_common(character, item, amount);
                        character->Send(reply);
                        return;
                    }
                }

                if (character->bank.size() >= lockermax)
                    return;

                amount = std::min<int>(amount, static_cast<int>(character->world->config["MaxBank"]));

                Character_Item newitem;
                newitem.id = item;
                newitem.amount = amount;

                character->bank.push_back(newitem);

                PacketBuilder reply = add_common(character, item, amount);
                character->Send(reply);
            }
        }

        if (character->HasPet && character->petinv_open)
        {
            UTIL_IFOREACH(character->pet->inventory, it)
            {
                if (it->id == item)
                {
                    if (it->amount + amount < 0)
                    {
                        return;
                    }

                    amount = std::min<int>(amount, static_cast<int>(character->world->config["MaxBank"]) - it->amount);

                    it->amount += amount;

                    PacketBuilder reply = add_common(character, item, amount);
                    character->Send(reply);
                    return;
                }
            }

            if (character->pet->inventory.size() >= lockermax)
            {
                return;
            }

            amount = std::min<int>(amount, static_cast<int>(character->world->config["MaxBank"]));

            Character_Item newitem;
            newitem.id = item;
            newitem.amount = amount;

            character->pet->inventory.push_back(newitem);

            PacketBuilder reply = add_common(character, item, amount);
            character->Send(reply);
        }
    }

    void Locker_Take(Character *character, PacketReader &reader)
    {
        if (character->trading) return;

        unsigned char x = reader.GetChar();
        unsigned char y = reader.GetChar();

        short item = reader.GetShort();

        if (util::path_length(character->x, character->y, x, y) <= 1 && !character->petinv_open)
        {
            if (character->map->GetSpec(x, y) == Map_Tile::BankVault)
            {
                UTIL_IFOREACH(character->bank, it)
                {
                    if (it->id == item)
                    {
                        int amount = it->amount;
                        int taken = character->CanHoldItem(it->id, amount);

                        character->AddItem(item, taken);

                        character->CalculateStats();

                        PacketBuilder reply(PACKET_LOCKER, PACKET_GET, 7 + character->bank.size() * 5);
                        reply.AddShort(item);
                        reply.AddThree(taken);
                        reply.AddChar(character->weight);
                        reply.AddChar(character->maxweight);

                        it->amount -= taken;

                        if (it->amount <= 0)
                            character->bank.erase(it);

                        UTIL_FOREACH(character->bank, item)
                        {
                            reply.AddShort(item.id);
                            reply.AddThree(item.amount);
                        }
                        character->Send(reply);

                        break;
                    }
                }
            }
        }

        if (character->HasPet && character->petinv_open)
        {
            UTIL_IFOREACH(character->pet->inventory, it)
            {
                if (it->id == item)
                {
                    int amount = it->amount;
                    int taken = character->CanHoldItem(it->id, amount);

                    character->AddItem(item, taken);

                    character->CalculateStats();

                    PacketBuilder reply(PACKET_LOCKER, PACKET_GET, 7 + character->pet->inventory.size() * 5);
                    reply.AddShort(item);
                    reply.AddThree(taken);
                    reply.AddChar(character->weight);
                    reply.AddChar(character->maxweight);

                    it->amount -= taken;

                    if (it->amount <= 0)
                        character->pet->inventory.erase(it);

                    UTIL_FOREACH(character->pet->inventory, item)
                    {
                        reply.AddShort(item.id);
                        reply.AddThree(item.amount);
                    }
                    character->Send(reply);

                    break;
                }
            }
        }
    }

    void Locker_Open(Character *character, PacketReader &reader)
    {
        character->petinv_open = false;

        unsigned char x = reader.GetChar();
        unsigned char y = reader.GetChar();

        if (util::path_length(character->x, character->y, x, y) <= 1)
        {
            if (character->map->GetSpec(x, y) == Map_Tile::BankVault)
            {
                if (static_cast<std::string>(character->world->config["LockerPin"]) == "yes")
                {
                    if (character->lockeraccess == true || character->lockerpin == 0000)
                    {
                        PacketBuilder reply(PACKET_LOCKER, PACKET_OPEN, 2 + character->bank.size() * 5);
                        reply.AddChar(x);
                        reply.AddChar(y);

                        UTIL_FOREACH(character->bank, item)
                        {
                            reply.AddShort(item.id);
                            reply.AddThree(item.amount);
                        }

                        character->Send(reply);
                        character->lockeraccess = false;

                        if (character->lockerpin == 0000)
                            character->ServerMsg("It's recommended to set a locker pin by using " + std::string(character->world->config["PlayerPrefix"]) + "setlockerpin <pin>.");
                    }
                    else
                    {
                        character->ServerMsg("You need to enter a pin to open your locker! Use: " + std::string(character->world->config["PlayerPrefix"]) + "openlocker <pin> to open it.");
                    }
                }
                else
                {
                    PacketBuilder reply(PACKET_LOCKER, PACKET_OPEN, 2 + character->bank.size() * 5);
                    reply.AddChar(x);
                    reply.AddChar(y);

                    UTIL_FOREACH(character->bank, item)
                    {
                        reply.AddShort(item.id);
                        reply.AddThree(item.amount);
                    }

                    character->Send(reply);
                }
            }
        }
    }

    void Locker_Buy(Character *character, PacketReader &reader)
    {
        if (character->trading) return;

        (void)reader;

        if (character->npc_type == ENF::Bank)
        {
            int cost = static_cast<int>(character->world->config["BankUpgradeBase"]) + character->bankmax * static_cast<int>(character->world->config["BankUpgradeStep"]);

            if (character->bankmax >= static_cast<int>(character->world->config["MaxBankUpgrades"]))
                return;

            if (character->HasItem(1) < cost)
                return;

            ++character->bankmax;
            character->DelItem(1, cost);

            PacketBuilder reply(PACKET_LOCKER, PACKET_BUY, 5);
            reply.AddInt(character->HasItem(1));
            reply.AddChar(character->bankmax);
            character->Send(reply);
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_LOCKER)
        Register(PACKET_ADD, Locker_Add, Playing);
        Register(PACKET_TAKE, Locker_Take, Playing);
        Register(PACKET_OPEN, Locker_Open, Playing);
        Register(PACKET_BUY, Locker_Buy, Playing);
    PACKET_HANDLER_REGISTER_END()
}
