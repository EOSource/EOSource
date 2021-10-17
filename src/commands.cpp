#include "commands.hpp"
#include "world.hpp"
#include "console.hpp"

Command::Command(short id, World *world)
{
    char namebuf[6];

    if (id < 0)
    {
        return;
    }

    this->id = id;
    this->exists = true;

    std::string filename = "";
    filename = world->config["CommandDir"].GetString();
    std::sprintf(namebuf, "%05i", id);
    filename.append(namebuf);
    filename.append(".ecf");
    std::FILE *fh = std::fopen(filename.c_str(), "rt");
    std::string buf;

    if (!fh)
    {
        Console::Err("Could not load file: %s", filename.c_str());
        this->exists = false;
        return;
    }

    std::fseek(fh, 0, SEEK_END);
    long size = ftell(fh);
    std::rewind(fh);

    buf.resize(size);
    std::fread(&buf[0], 1, size, fh);
    std::fclose(fh);

    Load(buf);
}

void Command::Load(const std::string source)
{
    bool inmain = false;

    bool inquotes = false;
    bool inparens = false;
    bool incomment = false;
    bool escape = false;
    bool semicolon = false;
    bool type_said = false;
    bool commandname_said = false;

    int character = 0;

    int quote_start = 0;
    int quote_end = 0;

    int paren_start = 0;
    int paren_end = 0;

    int comment_start = 0;

    std::vector<int> args;

    Call *call;
    Check *check;

    std::string tempstring;

    while (source[character] != '\0')
    {
        if (!incomment && !escape && (source[character] == '\"' || source[character] == '\''))
        {
            if (inquotes)
{
                quote_end = tempstring.size();
                inquotes = false;
            }
            else
            {
                quote_start = tempstring.size();
                inquotes = true;
            }
        }
        else if (source[character] == '\\')
        {
            if (source[character+1] == '\'' || source[character+1] == '\"')
                escape = true;
        }
        else if (!inquotes && (source[character] == '/' && source[character+1] == '/'))
        {
            incomment = true;
            comment_start = tempstring.size();
        }
        else if ((!incomment && !inquotes && ((source[character] == '\n' && !inmain) || source[character] == ' ')) || source[character] == '\t')
        {
            character++;
            continue;
        }
        else if (incomment && source[character] == '\n')
        {
            incomment = false;
            tempstring.erase(comment_start);
        }
        else if (!incomment && !inquotes && source[character] == '{')
        {
            if (tempstring.compare(0, 4, "main") == 0)
            {
                inmain = true;
            }

            tempstring.clear();
        }
        else if (!incomment && !inquotes && source[character] == '}')
        {
            if (!inmain)
                SyntaxError("Invalid brace: No starting brace");

            if (inmain) inmain = false;

            tempstring.clear();
        }
        else if (!incomment && !inquotes && source[character] == '(')
        {
            if (inparens)
                SyntaxError("Nested parentheses not allowed");

            inparens = true;
            paren_start = tempstring.size();
        }
        else if (!incomment && !inquotes && source[character] == ')')
        {
            if (!inparens)
                SyntaxError("Not in parentheses");

            inparens = false;
            paren_end = tempstring.size();
        }
        else if (!incomment && !inquotes && source[character] == ';')
        {
            semicolon = true;
        }
        else if (!incomment && !inquotes && source[character] == ',')
        {
            if (!inparens)
                SyntaxError("Invalid comma outside of parentheses");

            args.push_back(tempstring.size());
        }
        else
        {
            escape = false;

            if ((source[character] >= 'A' && source[character] <= 'Z') && !inquotes)
                tempstring += source[character] - 'A' + 'a';
            else
                tempstring += source[character];
        }

        if (inmain)
        {
            if (source[character] == '\n')
            {
                if (tempstring.compare(0, 11, "commandname") == 0)
                {
                    if (quote_start == 0 || quote_end == 0)
                        SyntaxError("No quotation marks around the commandname");

                    this->name = tempstring.substr(quote_start, quote_end);
                    commandname_said = true;
                    quote_start = quote_end = 0;
                }
                else if (tempstring.compare(0, 4, "type") == 0)
                {
                        if (quote_start == 0 || quote_end == 0)
                        SyntaxError("No quotation marks around the type");

                    this->type = tempstring.substr(quote_start, quote_end);
                    type_said = true;
                    quote_start = quote_end = 0;
                }
                else if (tempstring.compare(0, 4, "call") == 0)
                {
                    if (semicolon)
                    {
                        if (paren_start == 0 || paren_end == 0)
                            SyntaxError("No parentheses in call");

                        call = new Call();
                        call->name = tempstring.substr(4, paren_start - 4);
                        if (args.size() > 0)
                        {
                            size_t start, end;
                            for (size_t i = 0; i <= args.size(); ++i)
                            {
                                start = (i == 0 ? paren_start : args[i - 1]);
                                end = (i == args.size() ? paren_end : args[i]) - start;
                                call->args.push_back(*new util::variant(tempstring.substr(start, end)));
                            }
                        }
                        else
                        {
                            call->args.push_back(*new util::variant(tempstring.substr(paren_start, paren_end-paren_start)));
                        }

                        /*if (call->name.compare("giveitem") == 0 && (call->args.size() != 1 || call->args.size() != 2 || call->args.size() != 3))
                            SyntaxError("GiveItem requires between one and three parameters");
                        else if (call->name.compare("removeitem") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("RemoveItem requires two parameters");
                        else if (call->name.compare("giveexp") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("GiveExp requires one parameter");
                        else if (call->name.compare("setexp") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("SetExp requires one parameter");
                        else if (call->name.compare("givekarma") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("GiveKarma requires one parameter");
                        else if (call->name.compare("removekarma") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("RemoveKarma requires one parameter");
                        else if (call->name.compare("playsound") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("PlaySound requires one parameter");
                        else if (call->name.compare("settitle") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("SetTitle requires one parameter");
                        else if (call->name.compare("sethome") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("SetHome requires one parameter");
                        else if (call->name.compare("setpartner") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("SetPartner requires one parameter");
                        else if (call->name.compare("setrace") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("SetRace requires one parameter");
                        else if (call->name.compare("setclass") == 0 && (call->args.size() != 1 || call->args.size() != 2))
                            SyntaxError("SetClass requires one parameter");
                        else if (call->name.compare("quake") == 0 && (call->args.size() != 1 && call->args.size() != 2))
                            SyntaxError("Quake requires at least one parameter");
                        else if (call->name.compare("questbox") == 0 && (call->args.size() != 2 || call->args.size() != 3))
                            SyntaxError("QuestBox requires at least two parameters");
                        else if (call->name.compare("infobox") == 0 && (call->args.size() != 2 || call->args.size() != 3))
                            SyntaxError("InfoBox requires at least two parameters");*/

                        this->calls.push_back(*call);

                        paren_start = paren_end = 0;
                        tempstring.clear();
                        args.clear();
                        semicolon = false;
                    }
                    else if (paren_end != 0)
                    {
                        SyntaxError("Expected \';\'");
                        paren_start = paren_end = 0;
                        tempstring.clear();
                        args.clear();
                    }
                }
                else if (tempstring.compare(0, 5, "check") == 0)
                {
                    if (paren_start == 0 || paren_end == 0)
                        SyntaxError("No parentheses in check");

                    check = new Check();
                    check->name = tempstring.substr(5, paren_start - 5);

                    if (args.size() > 0)
                    {
                        size_t start, end;
                        for (size_t i = 0; i <= args.size(); ++i)
                        {
                            start = (i == 0 ? paren_start : args[i - 1]);
                            end = (i == args.size() ? paren_end : args[i]) - start;
                            check->args.push_back(*new util::variant(tempstring.substr(start, end)));
                        }
                    }
                    else
                    {
                        check->args.push_back(*new util::variant(tempstring.substr(paren_start, paren_end-paren_start)));
                    }

                    if (check->name.compare("item") == 0 && check->args.size() < 2)
                        SyntaxError("Item requires two parameters.");
                    else if (check->name.compare("stripped") == 0 && check->args.size() < 0)
                        SyntaxError("Stripped requires no parameters.");
                    else if (check->name.compare("location") == 0 && check->args.size() < 3)
                        SyntaxError("Location requires three parameters.");
                    else if (check->name.compare("gender") == 0 && check->args.size() < 1)
                        SyntaxError("Gender requires one parameter");
                    else if (check->name.compare("map") == 0 && check->args.size() < 1)
                        SyntaxError("Map requires one parameter");
                    else if (check->name.compare("cookinglevel") == 0 && check->args.size() < 1)
                        SyntaxError("CookingLevel requires one parameter");
                    else if (check->name.compare("fishinglevel") == 0 && check->args.size() < 1)
                        SyntaxError("FishingLevel requires one parameter");
                    else if (check->name.compare("mininglevel") == 0 && check->args.size() < 1)
                        SyntaxError("MiningLevel requires one parameter");
                    else if (check->name.compare("woodcuttinglevel") == 0 && check->args.size() < 1)
                        SyntaxError("WoodcuttingLevel requires one parameter");
                    else if (check->name.compare("level") == 0 && check->args.size() < 1)
                        SyntaxError("Level requires one parameter");
                    else if (check->name.compare("rebirth") == 0 && check->args.size() < 1)
                        SyntaxError("Rebirth requires one parameter");
                    else if (check->name.compare("admin") == 0 && check->args.size() < 1)
                        SyntaxError("Admin requires one parameter");
                    else if (check->name.compare("class") == 0 && check->args.size() < 1)
                        SyntaxError("Class requires one parameter");
                    else if (check->name.compare("home") == 0 && check->args.size() < 1)
                        SyntaxError("Home requires one parameter");
                    else if (check->name.compare("partner") == 0 && check->args.size() < 1)
                        SyntaxError("Partner requires one parameter");
                    else if (check->name.compare("title") == 0 && check->args.size() < 1)
                        SyntaxError("Title requires one parameter");
                    else if (check->name.compare("spell") == 0 && check->args.size() < 1)
                        SyntaxError("Spell requires one parameter");
                    else if (check->name.compare("race") == 0 && check->args.size() < 1)
                        SyntaxError("Race requires one parameter");

                    this->checks.push_back(*check);

                    paren_start = paren_end = 0;
                    tempstring.clear();
                    args.clear();
                }

                tempstring.clear();
            }
        }

        character++;
    }

    if (!commandname_said)
    SyntaxError("No commandname variable");

    if (!type_said)
    SyntaxError("No type variable");
}

void Command::Reload(World *world)
{
    this->calls.clear();
    this->checks.clear();
    this->name.clear();
    this->type.clear();
    char namebuf[6];
    this->exists = true;

    std::string filename = "";
    filename = world->config["CommandDir"].GetString();
    std::sprintf(namebuf, "%05i", id);
    filename.append(namebuf);
    filename.append(".ecf");
    std::FILE *fh = std::fopen(filename.c_str(), "rt");
    std::string buf;

    if (!fh)
    {
        Console::Err("Could not load Command file %s", filename.c_str());
        this->exists = false;
        return;
    }

    std::fseek(fh, 0, SEEK_END);
    long size = ftell(fh);
    std::rewind(fh);

    buf.resize(size);
    std::fread(&buf[0], 1, size, fh);
    std::fclose(fh);

    Load(buf);
}
