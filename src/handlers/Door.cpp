#include "handlers.hpp"

#include "../character.hpp"
#include "../map.hpp"

namespace Handlers
{
    void Door_Open(Character *character, PacketReader &reader)
    {
        int x = reader.GetChar();
        int y = reader.GetChar();

        character->map->OpenDoor(character, x, y);

        if (character->map->GetSpec(x, y) == Map_Tile::Door)
        {
            character->board = character->world->boards[9];

            Database_Result res = character->world->db.Query("SELECT `name`, `title`, `home`, `partner`, `class`, `gender`, `rebirth`, "
            "`level`, `exp`, `usage`, `guild` FROM `characters` WHERE `admin` <= $ ORDER BY `rebirth` DESC, `exp` DESC LIMIT 10", std::string(character->world->config["TopPlayerAccess"]).c_str());

            char res_size = 0;

            res_size = res.size();

            PacketBuilder builder(PACKET_BOARD, PACKET_OPEN, res_size + 2);
            builder.AddChar(9);
            builder.AddChar(res_size + 2);
            builder.AddShort(0);
            builder.AddByte(255);
            builder.AddBreakString(" ");
            builder.AddBreakString("                 Top 10 Players");
            builder.AddShort(1);
            builder.AddByte(255);
            builder.AddBreakString("(Name)");
            builder.AddBreakString("(Level)               (Experience)            (Rebirth)");

            int idcount = 1;

            character->board->last_id = 1;

            UTIL_FOREACH_REF(res, row)
            {
                ++idcount;

                builder.AddShort(idcount);
                builder.AddByte(255);

                std::string resname = row["name"];

                int reslvl = row["level"];
                int resexp = row["exp"];
                int resreb = row["rebirth"];

                std::string restitle = row["title"];
                std::string reshome = row["home"] ? row["home"] : "None";
                std::string respartner = row["partner"] ? row["partner"] : "None";

                ECF_Data& resclass = character->world->ecf->Get(util::to_int(row["class"]));

                std::string resgender;

                if (int(row["gender"]) == 0)
                {
                    resgender = "Female";
                }
                else
                {
                    resgender = "Male";
                }

                int resusage = row["usage"];

                std::string resguild = row["guild"];

                if (resguild.length() > 0)
                {
                    std::shared_ptr<Guild> resguildref = character->world->guildmanager->GetGuild(resguild);
                    resguild = "[" + resguildref->tag + "] - " + resguildref->name;
                }
                else
                {
                    resguild = "None";
                }

                std::string rusage = "";

                if (resusage >= 60)
                {
                    rusage += util::to_string(resusage / 60) + "hrs. ";
                }

                rusage += util::to_string(resusage % 60) + "min";

                std::string fillerlvl = "00000";
                std::string fillerexp = "0000000000";
                std::string fillerreb = "0000000";

                std::string rlvl = fillerlvl.substr(0, fillerlvl.length() - util::to_string(reslvl).length()) + util::to_string(reslvl);
                std::string rexp = fillerexp.substr(0, fillerexp.length() - util::to_string(resexp).length()) + util::to_string(resexp);
                std::string rreb = fillerreb.substr(0, fillerreb.length() - util::to_string(resreb).length()) + util::to_string(resreb);

                std::string resbody = util::ucfirst(resname) + " - [" + rusage + "] ";
                resbody[resbody.length() - 1] = 13;
                resbody += "Level: " + util::to_string(reslvl) + " ";
                resbody[resbody.length() - 1] = 13;
                resbody += "Exp: " + util::to_string(resexp) + " ";
                resbody[resbody.length() - 1] = 13;
                resbody += "Reborn: " + util::to_string(resreb) + " ";
                resbody[resbody.length() - 1] = 13;
                resbody += "Gender: " + resgender + " ";
                resbody[resbody.length() - 1] = 13;
                resbody += "Class: " + resclass.name + " ";

                resbody[resbody.length() - 1] = 13;

                if (restitle == "" || restitle == " ") resbody += "Title: None ";
                    else resbody += "Title: " + restitle + " ";

                resbody[resbody.length() - 1] = 13;

                if (reshome == "" || reshome == " ") resbody += "Home: None ";
                    else resbody += "Home: " + reshome + " ";

                resbody[resbody.length() - 1] = 13;

                if (resguild == "" || resguild == " ") resbody += "Guild: None ";
                    else resbody += "Guild: " + resguild + " ";

                resbody[resbody.length() - 1] = 13;

                if (respartner == "" || respartner == " ") resbody += "Partner: None ";
                    else resbody += "Partner: " + respartner + " ";

                std::string displaytext = rlvl + "                " + rexp + "           " + rreb;

                Board_Post *newpost = new Board_Post;

                newpost->id = ++character->board->last_id;
                newpost->author = util::ucfirst(resname);
                newpost->author_admin = ADMIN_PLAYER;
                newpost->subject = displaytext;
                newpost->body = resbody;
                newpost->time = Timer::GetTime();

                character->board->posts.push_front(newpost);

                builder.AddBreakString(util::ucfirst(resname));
                builder.AddBreakString(displaytext);
            }

            character->Send(builder);
        }
    }

    PACKET_HANDLER_REGISTER(PACKET_DOOR)
            Register(PACKET_OPEN, Door_Open, Playing);
    PACKET_HANDLER_REGISTER_END()
}
