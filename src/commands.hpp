#ifndef Command_HPP_INCLUDED
#define Command_HPP_INCLUDED

#include "util.hpp"
#include "util/variant.hpp"
#include "world.hpp"
#include "console.hpp"

#include "stdafx.h"

struct Check
{
    std::string name;
    std::vector<util::variant> args;

    std::string goto_state;
};

struct Call
{
    std::string name;
    std::vector<util::variant> args;
};

class Command
{
    private:
    void Load(const std::string source);

    void SyntaxError(const std::string error)
    {
        Console::Wrn("Syntax error loading Command %i: %s", this->id, error.c_str());
        this->exists = false;
    };

    public:

    bool exists;
    short id;

    std::string name;
    std::string type;

    std::vector<Check> checks;
    std::vector<Call> calls;

    Command(short id, World *world);

    void Reload(World *world);
};

#endif
