#ifndef GFX_H_INCLUDED
#define GFX_H_INCLUDED

struct GFXStruct
{
    ALLEGRO_BITMAP *gui[5];
};

class GFX
{
public:
    static bool Initialize();
    static void Destroy(ALLEGRO_BITMAP *bmp);

    static ALLEGRO_BITMAP *LoadFromResource(const char *filename, int resID , bool transparant);
};

extern GFXStruct gfx;

#endif
