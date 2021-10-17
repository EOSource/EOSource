#ifndef CHAT_H_INCLUDED
#define CHAT_H_INCLUDED

#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>

#include "util.hpp"

using namespace std;

struct ChatStruct
{
    int x;
    int y;
    int p;

    int p_line;
    int cp_line;

    std::string input;

    std::string chat[5000];
    std::string cchat[5000];

    int r[5000];
    int g[5000];
    int b[5000];
    int begin[5000];

    int cr[5000];
    int cg[5000];
    int cb[5000];
    int cp_id[5000];

    ALLEGRO_BITMAP *png;
};

class Chat
{
    public:

    static void ChatBox();
    static void InfoBox();

    static void Info(std::string text, int r, int g, int b);
};

extern ChatStruct chat;
#endif
