#include "handlers.hpp"

#include "../character.hpp"
#include "../map.hpp"

namespace Handlers
{
    void Face_Player(Character *character, PacketReader &reader)
    {
        Direction direction = static_cast<Direction>(reader.GetChar());

        if (character->sitting != SIT_STAND)
        {
            return;
        }

        if (direction <= 3)
        {
            character->map->Face(character, direction);
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_FACE)
        Register(PACKET_PLAYER, Face_Player, Playing, 0.12);
    PACKET_HANDLER_REGISTER_END()
}
