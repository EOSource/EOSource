#ifndef FWD_CONSOLE_HPP_INCLUDED
#define FWD_CONSOLE_HPP_INCLUDED

#include <string>

namespace Console
{
    void Out(std::string f, ...);
    void RedOut(std::string f, ...);
    void PurpleOut(std::string f, ...);
    void YellowOut(std::string f, ...);
    void GreenOut(std::string f, ...);

    void Wrn(std::string f, ...);
    void Err(std::string f, ...);
    void Dbg(std::string f, ...);
};

#endif
