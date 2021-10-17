#include <csignal>
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <vector>

#include "platform.h"
#include "version.h"

#include "character.hpp"
#include "config.hpp"
#include "console.hpp"
#include "eoclient.hpp"
#include "eoserver.hpp"
#include "packet.hpp"
#include "player.hpp"
#include "socket.hpp"
#include "world.hpp"
#include "eoserv_config.hpp"
#include "util.hpp"

#ifdef GUI
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_primitives.h>
#include "graphics.hpp"
#include "text.hpp"
#include "chat.hpp"
#endif

#ifdef WIN32
#include <windows.h>
#include "extra/ntservice.hpp"
#endif

volatile std::sig_atomic_t exiting_eoserv = false;
volatile std::sig_atomic_t rehash_eoserv = false;

volatile bool eoserv_running = true;

std::string get_timestr(std::string format = "%c")
{
    std::time_t rawtime;
    char timestr[256];
    std::time(&rawtime);
    std::strftime(timestr, 256, format.c_str(), std::localtime(&rawtime));

    return std::string(timestr);
}

static EOServer *eoserv_rehash_server = 0;

#ifdef GUI

string input;

int mouse_b;
int mouse_x;
int mouse_y;

int screen_width;
int screen_height;

void mouseaxes(ALLEGRO_MOUSE_EVENT *mouse)
{
    mouse_x = mouse->x * 650 / screen_width;
    mouse_y = mouse->y * 300 / screen_height;
}

void mousedown(ALLEGRO_MOUSE_EVENT *mouse)
{
    mouse_b |= mouse->button;
}

void mouseup(ALLEGRO_MOUSE_EVENT *mouse)
{
    mouse_b &= 0;
}

void mouseupdate()
{
    mouse_b &= 0x55555555;
}

int ButtonID;
int MenuButtonID;

void Button(int x, int y,int id, std::string text)
{
    int width = 80;
    int height = 20;

    if (mouse_x >= x && mouse_x <= x + width && mouse_y >= y && mouse_y <= y + height)
    {
        al_draw_filled_rectangle(x,y,width + x,y + height,al_map_rgb(237,225,213));
        al_draw_rectangle(x,y,width + x,y + height,al_map_rgb(0,0,0),1);

        Text::DrawMiddle(text.c_str(),0,0,0,x + 40, y + 3,NULL);

        if (mouse_b == 1)
        {
            MenuButtonID = id;
        }
    }
    else
    {
        al_draw_filled_rectangle(x,y,width + x,y + height,al_map_rgb(220,197,173));
        al_draw_rectangle(x,y,width + x,y + height,al_map_rgb(0,0,0),1);

        Text::DrawMiddle(text.c_str(),0,0,0,x + 40, y + 3,NULL);
    }
}

void ButtonSmall(int x, int y,int id, std::string text)
{
    int width = 68;
    int height = 20;

    if (mouse_x >= x && mouse_x <= x + width && mouse_y >= y && mouse_y <= y + height)
    {
        al_draw_filled_rectangle(x,y,width + x,y + height,al_map_rgb(237,225,213));
        al_draw_rectangle(x,y,width + x,y + height,al_map_rgb(0,0,0),1);

        Text::DrawMiddle(text.c_str(),0,0,0,x + 34, y + 3,NULL);

        if (mouse_b == 1)
        {
            ButtonID = id;
        }
    }
    else
    {
        al_draw_filled_rectangle(x,y,width + x,y + height,al_map_rgb(220,197,173));
        al_draw_rectangle(x,y,width + x,y + height,al_map_rgb(0,0,0),1);

        Text::DrawMiddle(text.c_str(),0,0,0,x + 34, y + 3,NULL);
    }
}

void ButtonMedium(int x, int y,int id, std::string text)
{
    int width = 80;
    int height = 20;

    if (mouse_x >= x && mouse_x <= x + width && mouse_y >= y && mouse_y <= y + height)
    {
        al_draw_filled_rectangle(x,y,width + x,y + height,al_map_rgb(237,225,213));
        al_draw_rectangle(x,y,width + x,y + height,al_map_rgb(0,0,0),1);

        Text::DrawMiddle(text.c_str(),0,0,0,x + 40, y + 3,NULL);

        if (mouse_b == 1)
        {
            ButtonID = id;
        }
    }
    else
    {
        al_draw_filled_rectangle(x,y,width + x,y + height,al_map_rgb(220,197,173));
        al_draw_rectangle(x,y,width + x,y + height,al_map_rgb(0,0,0),1);

        Text::DrawMiddle(text.c_str(),0,0,0,x + 40, y + 3,NULL);
    }
}

int lines;
int scroll;

string name[1000];
string selected_name;

void ViewPlayerList()
{
    int ivalue;

    int width = 15;
    int height = 15;

    int online = eoserv_rehash_server->world->characters.size();

    if (mouse_x >= 530 && mouse_x <= 530 + width && mouse_y >= 55 && mouse_y <= 55 + height)
    {
        al_draw_filled_triangle(530,55 + height,530 + width / 2,55,530 + width,55 + height,al_map_rgb(237,225,213));

        if (online - 15 - scroll > 0 && mouse_b == 1 && online > 15)
        {
            scroll += 1;
        }
    }
    else
    {
        al_draw_filled_triangle(530,55 + height,530 + width / 2,55,530 + width,55 + height,al_map_rgb(200,170,135));
    }

    if (mouse_x >= 530 && mouse_x <= 530 + width && mouse_y >= 228 && mouse_y <= 228 + height)
    {
        al_draw_filled_triangle(530,228,530 + width / 2,228 + height,530 + width,228,al_map_rgb(237,225,213));

        if (scroll > 0 && mouse_b == 1)
        {
            scroll -= 1;
        }
    }
    else
    {
        al_draw_filled_triangle(530,228,530 + width / 2,228 + height,530 + width,228,al_map_rgb(200,170,135));
    }

    if (mouse_b == 1)
    {
        ivalue = 0;

        for(int i = 0; i <= 100; i++)
        {
            name[i].clear();
        }

        UTIL_FOREACH(eoserv_rehash_server->world->characters, character)
        {
            name[ivalue] = character->SourceName();

            ivalue ++;
        }
    }

    for (int i = 0; i <= 15; i++)
    {
        if (online > 15)
        {
            lines = online - 15 - scroll + i;
        }
        else
        {
            lines = i;
        }

        if (name[lines].length() > 0)
        {
            if (selected_name == name[lines])
            {
                Text::Draw(name[lines].c_str(),181,140,91,110,55 + i * 12,NULL);
            }
            else
            {
                Text::Draw(name[lines].c_str(),0,0,0,110,55 + i * 12,NULL);
            }

            if (mouse_x >= 110 && mouse_x <= 110 + 160 && mouse_y >= 55 + i * 12 && mouse_y <= 55 + i * 12 + 10 && mouse_b == 1)
            {
                selected_name = name[lines];
            }
        }
    }
}
#endif

#ifdef SIGHUP
static void eoserv_rehash(int signal)
{
	(void)signal;

	rehash_eoserv = true;
}
#endif

static void eoserv_terminate(int signal)
{
	(void)signal;

	exiting_eoserv = true;
}

#ifndef DEBUG
static void eoserv_crash(int signal)
{
	const char *extype = "Unknown error";

	switch (signal)
	{
		case SIGSEGV: extype = "Segmentation fault"; break;
		case SIGFPE: extype = "Floating point exception"; break;

        #ifdef SIGBUS
		case SIGBUS: extype = "Dereferenced invalid pointer"; break;
        #endif

		case SIGILL: extype = "Illegal instruction"; break;
	}

	Console::Err("EOSERV is dying! %s", extype);

    #ifdef DEBUG
	std::signal(signal, SIG_DFL);
	std::raise(signal);
    #else
	std::exit(0);
    #endif
}
#endif

#ifdef WIN32
HANDLE eoserv_close_event;

static BOOL WINAPI eoserv_win_event_handler(DWORD event)
{
	(void)event;
	exiting_eoserv = true;

	WaitForSingleObject(eoserv_close_event, INFINITE);

	return TRUE;
}
#endif

static_assert(std::numeric_limits<unsigned char>::digits >= 8, "You cannot run this program (char is less than 8 bits)");
static_assert(std::numeric_limits<unsigned short>::digits >= 16, "You cannot run this program (short is less than 16 bits)");
static_assert(std::numeric_limits<unsigned int>::digits >= 32, "You cannot run this program (int is less than 32 bits)");

static void exception_test() throw()
{
	try
	{
		throw std::runtime_error("You cannot run this program. Exception handling is working incorrectly.");
	}
	catch (std::exception& e)
	{

	}
}

int EOSERV(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	EOSERV(argc, argv);
}

int EOSERV(int argc, char *argv[])
{
    #ifdef GUI
    std::FILE *fh = std::fopen("pthreadGC2.dll", "rb");

    if (!fh)
    {
        #ifndef EOSERV_LINUX
        int long Message = MessageBox(NULL, "PthreadGC2.dll is missing! Do you want to download it?", "Error!", 1);

        if (Message == 1)
            ShellExecute(NULL, "open", "http://eosource.net/files/pthreadGC2.dll", NULL, NULL, SW_SHOWNORMAL);
        #endif

        std::exit(0);
    }

    screen_width = 650;
    screen_height = 300;

    al_init();
    al_install_audio();
    al_init_acodec_addon();
    al_install_keyboard();
    al_init_font_addon();
    al_init_ttf_addon();
    al_init_image_addon();
    al_reserve_samples(1);
    al_init_primitives_addon();
    al_install_mouse();

    ALLEGRO_BITMAP *buffer;
    ALLEGRO_DISPLAY *display;
    ALLEGRO_EVENT_QUEUE *queue = NULL;
    ALLEGRO_TIMER *timer = NULL;
    ALLEGRO_EVENT event;
    ALLEGRO_BITMAP *icon = GFX::LoadFromResource("pthreadGC2.dll", 101, true);

    al_set_new_display_flags(ALLEGRO_RESIZABLE);
    display = al_create_display(650, 300);

    al_set_display_icon(display, icon);
    al_set_window_title(display, "EOSource");

    queue = al_create_event_queue();

    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, (ALLEGRO_EVENT_SOURCE*)al_get_mouse_event_source());

    buffer = al_create_bitmap(650,300);
    timer = al_create_timer(ALLEGRO_BPS_TO_SECS(20));

    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_timer_event_source(timer));
    al_start_timer(timer);

    Text::Initialize();
    GFX::Initialize();

    chat.png = GFX::LoadFromResource("pthreadGC2.dll", 102, true);
    #endif

	if (!std::numeric_limits<char>::is_signed) Console::Wrn("char is not signed, correct operation of the server cannot be guaranteed.");

	exception_test();

    #ifdef WIN32
	if (argc >= 2)
	{
		std::string mode(argv[1]);
		std::string name = "EOSERV";
		bool silent = false;

		if (argc >= 3)
		{
			name = argv[2];
		}

		if (argc >= 4)
		{
			if (std::string(argv[3]) == "silent")
			{
				silent = true;
			}
		}

		if (mode == "service")
		{
			char cwd[MAX_PATH];
			GetModuleFileName(0, cwd, MAX_PATH);

			char *lastslash = 0;

			for (char *p = cwd; *p != '\0'; ++p)
			{
				if (*p == '\\' || *p == '/')
				{
					lastslash = p;
				}
			}

			if (lastslash)
			{
				*(lastslash+1) = '\0';
			}

			SetCurrentDirectory(cwd);
			service_init(name.c_str());
		}
		else if (mode == "install")
		{
			if (service_install(name.c_str()))
			{
				if (!silent) MessageBox(0, "Service installed.", "EOSERV", MB_OK);
				return 0;
			}
			else
			{
				if (!silent) MessageBox(0, OSErrorString(), "EOSERV", MB_OK);
				return 1;
			}
		}
		else if (mode == "uninstall")
		{
			if (service_uninstall(name.c_str()))
			{
				if (!silent) MessageBox(0, "Service uninstalled.", "EOSERV", MB_OK);
				return 0;
			}
			else
			{
				if (!silent) MessageBox(0, OSErrorString(), "EOSERV", MB_OK);
				return 1;
			}
		}

		return 0;
	}
    #endif

    #ifdef SIGHUP
	std::signal(SIGHUP, eoserv_rehash);
    #endif

	std::signal(SIGABRT, eoserv_terminate);
	std::signal(SIGTERM, eoserv_terminate);
	std::signal(SIGINT, eoserv_terminate);

    #ifndef DEBUG
	std::signal(SIGSEGV, eoserv_crash);
	std::signal(SIGFPE, eoserv_crash);
    #ifdef SIGBUS
	std::signal(SIGBUS, eoserv_crash);
    #endif
	std::signal(SIGILL, eoserv_crash);
    #endif

    #ifdef WIN32
	eoserv_close_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (!SetConsoleCtrlHandler(static_cast<PHANDLER_ROUTINE>(eoserv_win_event_handler), TRUE))
	{
		Console::Err("Could not install Windows console event handler");
		Console::Err("$shutdown must be used to exit the server cleanly");
	}
    #endif

    #ifndef DEBUG_EXCEPTIONS
	try
	{
	    #endif
		Config config, aconfig;

		try
		{
			config.Read("config.ini");
		}
		catch (std::runtime_error &e)
		{
			Console::Wrn("Could not load config.ini - using defaults");
		}

		try
		{
			aconfig.Read("admin.ini");
		}
		catch (std::runtime_error &e)
		{
			Console::Err("Could not load admin.ini - using defaults");
		}

		eoserv_config_validate_config(config);

		Console::Styled[1] = Console::Styled[0] = config["StyleConsole"];

        #ifndef EOSERV_LINUX
        SetConsoleTitle("EOSource");
        #endif;

        #ifdef CONSOLE
        if (std::string(config["ConsoleLogo"]) == "yes")
        {
        std::puts("\
  ______ ____   _____\n\
 |  ____/ __ \\ / ____|\n\
 | |__ | |  | | (___   ___  _   _ _ __ ___ ___\n\
 |  __|| |  | | \___ \\ / _ \\| | | | '__/ __/ _ \\ \n\
 | |___| |__| |____) | (_) | |_| | | | (_|  __/\n\
 |______\\____/|_____/ \\___/ \\__,_|_|  \\___\\___| \
        \n");
        }
        #endif

        #ifdef DEBUG
		Console::Wrn("This is a debug build and shouldn't be used for live servers.");
        #endif

		{
			std::time_t rawtime;
			char timestr[256];
			std::time(&rawtime);
			std::strftime(timestr, 256, "%c", std::localtime(&rawtime));

			std::string logerr = config["LogErr"];

			if (!logerr.empty() && logerr.compare("-") != 0)
			{
				if (!std::freopen(logerr.c_str(), "a", stderr))
				{
					Console::Err("Failed to redirect errors.");
				}
				else
				{
					Console::Styled[Console::STREAM_ERR] = false;
					std::fprintf(stderr, "\n\n--- %s ---\n\n", timestr);
				}

				if (!std::setvbuf(stderr, 0, _IONBF, 0) == 0)
				{
					Console::Wrn("Failed to change stderr buffer settings");
				}
			}

			std::string logout = config["LogOut"];

			if (!logout.empty() && logout.compare("-") != 0)
			{
				if (!std::freopen(logout.c_str(), "a", stdout))
				{
					Console::Err("Failed to redirect output.");
				}
				else
				{
					Console::Styled[Console::STREAM_OUT] = false;
					std::printf("\n\n--- %s ---\n\n", timestr);
				}

				if (!std::setvbuf(stdout, 0, _IONBF, 0) == 0)
				{
					Console::Wrn("Failed to change stdout buffer settings");
				}
			}
		}

		std::array<std::string, 6> dbinfo;

		dbinfo[0] = std::string(config["DBType"]);
		dbinfo[1] = std::string(config["DBHost"]);
		dbinfo[2] = std::string(config["DBUser"]);
		dbinfo[3] = std::string(config["DBPass"]);
		dbinfo[4] = std::string(config["DBName"]);
		dbinfo[5] = std::string(config["DBPort"]);

		EOServer server(static_cast<std::string>(config["Host"]), static_cast<int>(config["Port"]), dbinfo, config, aconfig);
        eoserv_rehash_server = &server;
        server.Listen(int(config["MaxConnections"]), int(config["ListenBacklog"]));

		bool tables_exist = false;
		bool tried_install = false;

		while (!tables_exist)
		{
			bool try_install = false;

			try
			{
				Database_Result accounts = server.world->db.Query("SELECT COUNT(1) AS `count` FROM `accounts`");
				Database_Result characters = server.world->db.Query("SELECT COUNT(1) AS `count` FROM `characters`");
				Database_Result admins = server.world->db.Query("SELECT COUNT(1) AS `count` FROM `characters` WHERE `admin` > 0");
				Database_Result guilds = server.world->db.Query("SELECT COUNT(1) AS `count` FROM `guilds`");
				Database_Result ipbans = server.world->db.Query("SELECT COUNT(1) AS `count` FROM `bans` WHERE `ip`");
				Database_Result members = server.world->db.Query("SELECT COUNT(1) AS `count` FROM `characters` WHERE `member` > 0");

                #ifdef GUI
                Chat::Info("Database Type: " + std::string(config["DBType"]),255,255,255);
                Chat::Info("Accounts: "  + util::to_string(int(accounts.front()["count"])),255,255,255);
                Chat::Info("Characters: " + util::to_string(int(characters.front()["count"])),255,255,255);
                Chat::Info("Staff Members: " + util::to_string(int(admins.front()["count"])),255,255,255);
                Chat::Info("Guilds: " + util::to_string(int(guilds.front()["count"])),255,255,255);
                Chat::Info("Banned IPs: " + util::to_string(int(ipbans.front()["count"])),255,255,255);
                Chat::Info("Members: " + util::to_string(int(members.front()["count"])),255,255,255);
                #else
                Console::Out("Database Type: " + std::string(config["DBType"]));
                Console::Out("Accounts: %i", int(accounts.front()["count"]));
                Console::Out("Characters: %i", int(characters.front()["count"]));
                Console::Out("Staff Members: %i", int(admins.front()["count"]));
                Console::Out("Guilds: %i", int(guilds.front()["count"]));
                Console::Out("Banned IPs: %i", int(ipbans.front()["count"]));
                Console::Out("Members: %i", int(members.front()["count"]));
                #endif

                #ifdef GUI
                Chat::Info("Listening on " + static_cast<std::string>(config["Host"]) + ":" + util::to_string(static_cast<int>(config["Port"])) + " (0/" + util::to_string(static_cast<int>(config["MaxConnections"])) + " connections)", 234,230,157);
                #else
                Console::YellowOut("Listening on %s:%i (0/%i connections)", std::string(config["Host"]).c_str(), int(config["Port"]), int(config["MaxConnections"]));
                #endif

                if (server.world->config["AutoAdmin"])
                {
                    #ifdef GUI
                    Chat::Info("All characters will be given GM admin status.",255,255,255);
                    #else
                    Console::Out("All characters will be given GM admin status.");
                    #endif
                }

				server.world->UpdateAdminCount(int(admins.front()["count"]));

				tables_exist = true;
			}
			catch (Database_Exception &e)
			{
			    #ifdef DEBUG
				Console::Dbg("Install check: %s: %s", e.what(), e.error());
                #endif // DEBUG

				if (tried_install)
				{
					Console::Err("Could not find or install tables.");
					Console::Err(e.error());
					std::exit(0);
				}

				try_install = true;
			}

			if (try_install)
			{
				tried_install = true;
				Console::Wrn("A required table is missing. Attempting to execute install.sql");

				try
				{
					server.world->CommitDB();
					server.world->db.ExecuteFile(config["InstallSQL"]);
					server.world->BeginDB();
				}
				catch (Database_Exception& e)
				{
					Console::Err("Could not install tables.");
					Console::Err(e.error());
					std::exit(0);
				}
			}
		}

		while (eoserv_running)
		{
		    #ifdef GUI
            chat.x = mouse_x; chat.y = mouse_y; chat.p = mouse_b;

            al_set_target_bitmap(buffer);
            al_lock_bitmap(buffer, ALLEGRO_PIXEL_FORMAT_ANY, 0);
            al_clear_to_color(al_map_rgb(200,170,135));
            al_wait_for_event(queue, &event);

            switch (event.type)
            {
                case ALLEGRO_EVENT_DISPLAY_RESIZE:
                {
                    if (event.display.width > 0)
                        screen_width = event.display.width;

                    if (event.display.height > 0)
                        screen_height = event.display.height;
                }
                break;

                case ALLEGRO_EVENT_KEY_CHAR:
                {
                    ALLEGRO_USTR *text = al_ustr_new("");

                    al_ustr_append_chr(text, event.keyboard.unichar);

                    if (event.keyboard.keycode != ALLEGRO_KEY_ENTER && event.keyboard.keycode != ALLEGRO_KEY_TAB)
                    {
                        if (MenuButtonID == 2)
                        {
                            if (event.keyboard.keycode == ALLEGRO_KEY_BACKSPACE)
                            {
                                if (chat.input.length() > 0)
                                    chat.input.resize(chat.input.size() - 1);
                            }
                            else if (Text::Width(chat.input) <= 438)
                            {
                                chat.input += al_cstr(text);
                            }
                        }
                    }
                    else if (event.keyboard.keycode == ALLEGRO_KEY_ENTER)
                    {
                        std::string message = chat.input.c_str();

                        if (message.find_first_of(std::string(server.world->config["AdminPrefix"])) == 0)
                        {
                            if (message == "$rehash")
                            {
                                Chat::Info("Config reloaded.",255,255,255);
                                server.world->Rehash();
                                Text::UpdateChat("Config reloaded.", 0,0,0,1);
                            }
                            else if (message == "$request")
                            {
                                Chat::Info("Quests reloaded.",255,255,255);
                                server.world->ReloadQuests();
                                Text::UpdateChat("Quests reloaded.", 0,0,0,1);
                            }
                            else if (message == "$repub")
                            {
                                Chat::Info("Pubs reloaded.",255,255,255);
                                server.world->ReloadPub();
                                Text::UpdateChat("Pubs reloaded.", 0,0,0,1);
                            }
                            else if (message == "$uptime")
                            {
                                Text::UpdateChat(util::timeago(server.start, Timer::GetTime()), 0,0,0,1);
                            }
                            else
                            {
                                Text::UpdateChat("Please enter a different command..", 0,0,0,1);
                            }
                        }
                        else
                        {
                            server.world->AnnounceMsg(0, chat.input.c_str(), true);
                            Text::UpdateChat(chat.input, 0,0,0,4);
                        }

                        chat.input.clear();
                            ButtonID = 0;
                    }
                }
                break;

                case ALLEGRO_EVENT_DISPLAY_CLOSE:
                exiting_eoserv = true;
                break;

                case ALLEGRO_EVENT_MOUSE_AXES:
                mouseaxes(&event.mouse);
                break;

                case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
                mousedown(&event.mouse);
                mouse_b = 1;
                break;

                case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
                mouseup(&event.mouse);
                break;

                case ALLEGRO_EVENT_TIMER:
                mouse_b = 0;
                text.amount ++;

                if (text.amount == 8)
                {
                    text.flikker = true;
                }
                else if (text.amount == 16)
                {
                    text.flikker = false;
                    text.amount = 0;
                }
                break;
            }

            Button(180,14,0,"Console");
            Button(270,14,1,"Commands");
            Button(360,14,2,"Chatting");

            if (MenuButtonID == 0)
            {
                al_draw_bitmap(gfx.gui[3],5,254,0);
                al_draw_bitmap(gfx.gui[4],610,255,0);

                al_draw_filled_rectangle(91,250,560,50,al_map_rgb(0,0,0));

                Chat::InfoBox();
                chat.input.clear();

                Text::Draw("EOSERV revision 449",0,0,0,250,285, NULL);
            }

            if (MenuButtonID == 1)
            {
                al_draw_bitmap(gfx.gui[3],95,3,0);
                al_draw_bitmap(gfx.gui[4],519,4,0);

                chat.input.clear();

                ButtonMedium(562,51,1,"Promote");
                ButtonMedium(562,73,2,"Demote");
                ButtonMedium(562,95,3,"Ban");
                ButtonMedium(562,117,4,"Kick");
                ButtonMedium(562,161 - 22,5,"Jail");
                ButtonMedium(562,183 - 22,6,"Mute");
                ButtonMedium(562,205 - 22,7,"Wall");
                ButtonMedium(562,227 - 22,15,"Freeze");
                ButtonMedium(562,249 - 22,16,"Unfreeze");

                ButtonMedium(8,73 - 22,8,"Silent Promote");
                ButtonMedium(8,95 - 22,9,"Silent Demote");
                ButtonMedium(8,117 - 22,10,"Silent Ban");
                ButtonMedium(8,139 - 22,11,"Silent Kick");
                ButtonMedium(8,161 - 22,12,"Silent Jail");
                ButtonMedium(8,183 - 22,13,"Silent Mute");
                ButtonMedium(8,205 - 22,14,"Silent Wall");
                ButtonMedium(8,227 - 22,17,"Silent Freeze");
                ButtonMedium(8,249 - 22,18,"Silent Unfreeze");

                Text::Draw(("Online characters: " + util::to_string(int(server.world->characters.size()))).c_str(), 0,0,0,540,287, NULL);

                if (selected_name.length() > 0 && server.world->characters.size() > 0)
                {
                    Character *victim = server.world->GetCharacter(selected_name);

                    if (victim && mouse_b == 1)
                    {
                        if (ButtonID == 1)
                        {
                            server.world->ServerMsg("Attention!! " + victim->SourceName() + " has been promoted to High Game Master!");

                            victim->player->character->admin = ADMIN_HGM;
                            victim->world->IncAdminCount();
                            victim->Save();
                        }
                        else if (ButtonID == 2)
                        {
                            server.world->ServerMsg("Attention!! " + victim->SourceName() + " has been demoted to a player!");

                            victim->player->character->admin = ADMIN_PLAYER;
                            victim->world->DecAdminCount();
                            victim->Save();
                        }
                        else if (ButtonID == 3)
                        {
                            server.world->Ban(victim, victim, (util::tdparse(server.world->config["DefaultBanLength"])), true);
                        }
                        else if (ButtonID == 4)
                        {
                            server.world->Kick(victim, victim, true);
                        }
                        else if (ButtonID == 5)
                        {
                            server.world->Jail(victim, victim, true);
                        }
                        else if (ButtonID == 6)
                        {
                            victim->Mute("Server");
                                server.world->ServerMsg("Attention!! " + selected_name + " was muted by " + "Server" + " - [Muted]");
                        }
                        else if (ButtonID == 7)
                        {
                            server.world->Wall(victim, victim, true);
                        }
                        else if (ButtonID == 8)
                        {
                            victim->player->character->admin = ADMIN_HGM;
                            victim->world->IncAdminCount();
                        }
                        else if (ButtonID == 9)
                        {
                            victim->player->character->admin = ADMIN_PLAYER;
                            victim->world->DecAdminCount();
                        }
                        else if (ButtonID == 10)
                        {
                            server.world->Ban(victim, victim, (util::tdparse(server.world->config["DefaultBanLength"])), false);
                        }
                        else if (ButtonID == 11)
                        {
                            server.world->Kick(victim, victim, true);
                        }
                        else if (ButtonID == 12)
                        {
                            server.world->Jail(victim, victim, false);
                        }
                        else if (ButtonID == 13)
                        {
                            victim->Mute("Server");
                        }
                        else if (ButtonID == 14)
                        {
                            server.world->Wall(victim, victim, false);
                        }
                        else if (ButtonID == 15)
                        {
                            server.world->ServerMsg("Attention!! " + selected_name + " has been frozen by " + "Server" + " - [Frozen]");

                            PacketBuilder reply;
                            reply.SetID(PACKET_WALK, PACKET_CLOSE);
                            victim->Send(reply);
                        }
                        else if (ButtonID == 16)
                        {
                            server.world->ServerMsg("Attention!! " + selected_name + " has been unfrozen by " + "Server" + " - [Unfrozen]");

                            PacketBuilder reply;
                            reply.SetID(PACKET_WALK, PACKET_OPEN);
                            victim->Send(reply);
                        }
                        else if (ButtonID == 17)
                        {
                            PacketBuilder reply;
                            reply.SetID(PACKET_WALK, PACKET_CLOSE);
                            victim->Send(reply);
                        }
                        else if (ButtonID == 18)
                        {
                            PacketBuilder reply;
                            reply.SetID(PACKET_WALK, PACKET_OPEN);
                            victim->Send(reply);
                        }
                    }

                    ButtonID = 0;
                }

                al_draw_filled_rectangle(100,250,550,50,al_map_rgb(220,197,173));
                al_draw_rectangle(100,250,550,50,al_map_rgb(0,0,0),1);

                ViewPlayerList();

                Text::Draw(("Selected character: " + (selected_name == "" ? "None" : util::ucfirst(selected_name))).c_str(), 0,0,0,245,252, NULL);
            }

            if (MenuButtonID == 2)
            {
                al_draw_bitmap(gfx.gui[3],5,254,0);
                al_draw_bitmap(gfx.gui[4],610,255,0);

                ButtonSmall(90,226,13,"Announce");
                ButtonSmall(166,226,14,"Server");
                ButtonSmall(243,226,15,"Global");
                ButtonSmall(320,226,16,"Admin");
                ButtonSmall(395,226,17,"Status");
                ButtonSmall(471,226,18,"Command");

                al_draw_filled_rectangle(90,217,540,50,al_map_rgb(220,197,173));
                al_draw_rectangle(90,217,540,50,al_map_rgb(0,0,0),1);

                al_draw_filled_rectangle(90,273,540,253,al_map_rgb(220,197,173));
                al_draw_rectangle(90,273,540,253,al_map_rgb(0,0,0),1);

                if (Text::Width(chat.input) < 440) input = chat.input;

                if (text.flikker == true)
                {
                    al_draw_line(94 + Text::Width(input.c_str()),254 + 2,94 + Text::Width(input.c_str()),254 + 15, al_map_rgb(200,170,135), 2);
                }

                Text::Draw(input.c_str(),0,0,0,93,257,NULL);

                Chat::ChatBox();

                if (ButtonID != 0 && mouse_b == 1)
                {
                    std::string message = chat.input.c_str();

                    if (message != "")
                    {
                        if (ButtonID == 13)
                        {
                            server.world->AnnounceMsg(0, chat.input.c_str(), true);
                            Text::UpdateChat(chat.input, 0,0,0,4);
                        }
                        else if (ButtonID == 14)
                        {
                            server.world->ServerMsg(chat.input.c_str());
                            Text::UpdateChat(chat.input, 0,0,0,6);
                        }
                        else if (ButtonID == 15)
                        {
                            server.world->Msg(0, chat.input.c_str(), true);
                            Text::UpdateChat(chat.input, 0,0,0,4);
                        }
                        else if (ButtonID == 16)
                        {
                            server.world->AdminMsg(0, chat.input.c_str(), true);
                            Text::UpdateChat(chat.input, 0,0,0,12);
                        }
                        else if (ButtonID == 17)
                        {
                            server.world->StatusMsg(chat.input.c_str());
                            Text::UpdateChat(chat.input, 0,0,0,1);
                        }
                        else if (ButtonID == 18)
                        {
                            if (message == "$rehash")
                            {
                                Chat::Info("Config reloaded.",255,255,255);
                                server.world->Rehash();
                                Text::UpdateChat("Config reloaded.", 0,0,0,1);
                            }
                            else if (message == "$request")
                            {
                                Chat::Info("Quests reloaded.",255,255,255);
                                server.world->ReloadQuests();
                                Text::UpdateChat("Quests reloaded.", 0,0,0,1);
                            }
                            else if (message == "$repub")
                            {
                                Chat::Info("Pubs reloaded.",255,255,255);
                                server.world->ReloadPub();
                                Text::UpdateChat("Pubs reloaded.", 0,0,0,1);
                            }
                            else if (message == "$uptime")
                            {
                                Text::UpdateChat(util::timeago(server.start, Timer::GetTime()), 0,0,0,1);
                            }
                            else
                            {
                                Text::UpdateChat("Please enter a different command..", 0,0,0,1);
                            }
                        }

                        chat.input.clear();
                        ButtonID = 0;
                    }
                }
            }

            al_set_target_bitmap(al_get_backbuffer(display));
            al_draw_bitmap(buffer, 0,0,0);
            al_flip_display();
            #endif

			if (exiting_eoserv)
			{
				Console::Out("Exiting EOSERV");
				exiting_eoserv = false;
				break;
			}

			if (rehash_eoserv)
			{
				Console::Out("Reloading config");
				rehash_eoserv = false;
				server.world->Rehash();
			}

			server.Tick();
		}
		#ifndef DEBUG_EXCEPTIONS
	}
	catch (Socket_Exception &e)
	{
		Console::Err("%s: %s", e.what(), e.error());
		return 1;
	}
	catch (Database_Exception &e)
	{
		Console::Err("%s: %s", e.what(), e.error());
		return 1;
	}
	catch (std::runtime_error &e)
	{
		Console::Err("Runtime Error: %s", e.what());
		return 1;
	}
	catch (std::logic_error &e)
	{
		Console::Err("Logic Error: %s", e.what());
		return 1;
	}
	catch (std::exception &e)
	{
		Console::Err("Uncaught Exception: %s", e.what());
		return 1;
	}
	catch (...)
	{
		Console::Err("Uncaught Exception");
		return 1;
	}
	#endif

    #ifdef WIN32
	::SetEvent(eoserv_close_event);
    #endif

	return 0;
}
