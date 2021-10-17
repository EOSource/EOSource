#ifndef TEXT_H_INCLUDED
#define TEXT_H_INCLUDED

#include <iostream>
#include <string>

using namespace std;

struct TextStruct
{
    bool flikker;
    int amount;
};

class Text
{
    public:

    static void Initialize();
    static void Destroy();

    static void Draw(const char *text,int r,int g,int b, int x, int y, ...);
    static void DrawMiddle(const char *text,int r,int g,int b, int x, int y, ...);
    static void UpdateChat(std::string text, int r, int g ,int b, int id);

    static int Width(string message);
};

extern TextStruct text;

#endif
