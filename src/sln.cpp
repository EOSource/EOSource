#include "sln.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

#include <pthread.h>

#include "console.hpp"
#include "eoserver.hpp"
#include "nanohttp.hpp"
#include "timer.hpp"
#include "world.hpp"
#include "character.hpp"

#include "version.h"

struct SLN_Request
{
	SLN* sln;
	std::string url;
	std::string bind;
	std::string host;
	int period;

	HTTP* http;
	bool timeout;

	SLN_Request()
		: sln(0)
		, period(600)
		, http(0)
		, timeout(false)
	{ }

	~SLN_Request()
	{
		if (this->http) delete this->http;
	}
};

SLN::SLN(EOServer* server)
{
	this->server = server;
	this->Request();
}

void SLN::Request()
{
	SLN_Request* eosource = new SLN_Request;

	eosource->sln = this;

	eosource->url += "http://eosource.net/sln.php?software=EOSource&version=" + HTTP::URLEncode("7.0");
	eosource->url += std::string("&retry=") + HTTP::URLEncode(util::to_string(int(this->server->world->config["SLNPeriod"])));

	if (std::string(this->server->world->config["SLNHost"]).length() > 0)
	{
		eosource->url += std::string("&hostname=") + HTTP::URLEncode(this->server->world->config["SLNHost"]);
	}

	eosource->url += std::string("&port=") + HTTP::URLEncode(this->server->world->config["Port"]);
	eosource->url += std::string("&name=") + HTTP::URLEncode(this->server->world->config["ServerName"]);

	if (std::string(this->server->world->config["SLNSite"]).length() > 0)
	{
		eosource->url += std::string("&website=") + HTTP::URLEncode(this->server->world->config["SLNSite"]);
	}

	if (std::string(this->server->world->config["SLNZone"]).length() > 0)
	{
		eosource->url += std::string("&zone=") + HTTP::URLEncode(this->server->world->config["SLNZone"]);
	}

	if (this->server->world->config["GlobalPK"])
	{
		eosource->url += std::string("&pk=1");
	}

	if (this->server->world->config["Deadly"])
	{
		eosource->url += std::string("&deadly=1");
	}

	if (std::string(this->server->world->config["YouTube"]).length() > 0)
    {
        eosource->url += std::string("&youtube=") + HTTP::URLEncode(this->server->world->config["YouTube"]);
    }

    if (std::string(this->server->world->config["FaceBook"]).length() > 0)
    {
        eosource->url += std::string("&facebook=") + HTTP::URLEncode(this->server->world->config["FaceBook"]);
    }

    if (std::string(this->server->world->config["Twitter"]).length() > 0)
    {
        eosource->url += std::string("&twitter=") + HTTP::URLEncode(this->server->world->config["Twitter"]);
    }

    if (std::string(this->server->world->config["GooglePlus"]).length() > 0)
    {
        eosource->url += std::string("&googleplus=") + HTTP::URLEncode(this->server->world->config["GooglePlus"]);
    }

	eosource->bind = std::string(this->server->world->config["SLNBind"]);
	eosource->host = std::string(this->server->world->config["Host"]);
	eosource->period = int(this->server->world->config["SLNPeriod"]);

	static pthread_t thread;

	if (pthread_create(&thread, 0, SLN::RequestThread, eosource) != 0)
	{
		throw std::runtime_error("Failed to create EOSource request thread");
	}

	pthread_detach(thread);
}

void* SLN::RequestThread(void* void_request)
{
	SLN_Request* request(static_cast<SLN_Request*>(void_request));

	try
	{
		Clock clock(5000);
		double start_time = clock.GetTime();

		try
		{
			if (request->bind == "0")
			{
				request->http = HTTP::RequestURL(request->url);
			}
			else if (request->bind == "1")
			{
				request->http = HTTP::RequestURL(request->url, IPAddress(request->host));
			}
			else
			{
				request->http = HTTP::RequestURL(request->url, IPAddress(request->bind));
			}
		}
		catch (Socket_Exception &e)
		{
			Console::Err(e.error());
			goto end;
		}
		catch (...)
		{
			Console::Err("There was a problem trying to make the HTTP request...");
			goto end;
		}

		double start_adjust_time = clock.GetTime();

		while (!request->http->Done())
		{
			request->http->Tick(0.1);

			if (start_time + 30.0 < clock.GetTime())
			{
				request->timeout = true;
				break;
			}
		}

		request->period -= std::floor(clock.GetTime() - start_adjust_time);

		if (request->period < 45 || request->timeout)
			request->period = 45;

	}
	catch (...)
	{
		Console::Wrn("There was an unknown SLN error");
	}

end:
	TimeEvent* event = new TimeEvent(SLN::TimedCleanup, request, 0.0, 1);
	request->sln->server->world->timer.Register(event);

	return 0;
}

void SLN::TimedCleanup(void* void_request)
{
	SLN_Request* request(static_cast<SLN_Request*>(void_request));

    if (!request->http)
 	{
		Console::Wrn("SLN check-in failed: HTTP connection failed.");
	}
	else if (request->timeout)
	{
 		Console::Wrn("SLN check-in failed: HTTP request timed out.");
 	}
	else if ((request->http->StatusCode() / 100) != 2)
	{
		Console::Wrn("SLN check-in failed: negative HTTP response.");
	}
	else
	{
		try
		{
			std::vector<std::string> lines = util::explode("\r\n", request->http->Response());
			UTIL_FOREACH(lines, line)
			{
				if (line.length() == 0)
				{
					continue;
				}

				std::vector<std::string> parts = util::explode('\t', line);

				int code = util::to_int(parts.at(0));
				int maincode = code / 100;

				std::string errmsg = std::string("(") + parts.at(0) + ") ";
				bool resolved = false;

				switch (maincode)
				{
					case 1:
                    switch (code)
                    {
                        case 104:
                        request->period = util::to_int(parts.at(2));
                        break;
                    }
                    break;

					case 2:
                    #ifdef EOSERV_LINUX
                    Console::Err("You have been banished from using this software.");
                    #else
					MessageBox(NULL, "You have been banished from using this software.", "Error!", MB_SYSTEMMODAL);
					#endif // EOSERV_LINUX

					std::exit(0);
                    break;

					case 3:
                    errmsg += "SLN Update Warning: ";
                    switch (code)
                    {
                        case 300:
                        errmsg += parts.at(4);

                        if (parts.at(2) == "retry")
                        {
                            int old_period = int(request->sln->server->world->config["SLNPeriod"]);
                            int new_period = util::to_int(parts.at(3));
                            request->sln->server->world->config["SLNPeriod"] = new_period;
                            request->period += new_period - old_period;
                            resolved = true;
                        }
                        else if (parts.at(2) == "name")
                        {
                            request->sln->server->world->config["ServerName"] = parts.at(3);
                            resolved = true;
                        }
                        else if (parts.at(2) == "url")
                        {
                            request->sln->server->world->config["SLNSite"] = parts.at(3);
                            resolved = true;
                        }
                        break;

                        case 301:
                        errmsg += parts.at(2);
                        break;

                        case 302:
                        errmsg += parts.at(2);
                        break;

                        default:
                        errmsg += "Unknown error code";
                        break;
                    }

                    Console::Wrn("%s", errmsg.c_str());

                    request->sln->server->world->AdminMsg(0, errmsg, ADMIN_HGM);

                    if (resolved)
                    {
                        request->sln->server->world->AdminMsg(0, "EOSERV has automatically resolved this message and the next check-in should succeed.", ADMIN_HGM);
                    }
                    break;

					case 4:
                    errmsg += "SLN Update Client Error: ";
                    switch (code)
                    {
                        case 400:
                        errmsg += parts.at(3);
                        break;

                        case 401:
                        errmsg += parts.at(3);

                        if (parts.at(2) == "url")
                        {
                            request->sln->server->world->config["SLNSite"] = "";
                            resolved = true;
                        }
                        break;

                        case 402:
                        errmsg += parts.at(2);
                        break;

                        case 403:
                        errmsg += parts.at(2);
                        break;

                        case 404:
                        errmsg += parts.at(2);
                        break;

                        default:
                        errmsg += "Unknown error code";
                        break;
                    }

                    Console::Wrn("%s", errmsg.c_str());

                    request->sln->server->world->AdminMsg(0, errmsg, ADMIN_HGM);

                    if (resolved)
                    {
                        request->sln->server->world->AdminMsg(0, "EOSERV has automatically resolved this message and the next check-in should succeed.", ADMIN_HGM);
                    }
                    break;

					case 5:
                    errmsg += "SLN Update Server Error: ";

                    switch (code)
                    {
                        case 500:
                        errmsg += parts.at(2);
                        break;

                        default:
                        errmsg += "Unknown error code";
                        break;
                    }

                    Console::Wrn("%s", errmsg.c_str());

                    request->sln->server->world->AdminMsg(0, errmsg, ADMIN_HGM);
                    break;
				}
			}
		}
		catch (...)
		{
			Console::Wrn("There was an error parsing the SLN reply");
		}
	}

	if (request->period > 900)
	{
	    request->period = 900;
	}

    if (request->period != 45)
    {
        TimeEvent* event = new TimeEvent(SLN::TimedRequest, request->sln, request->period, 1);
        request->sln->server->world->timer.Register(event);
    }

	delete request;
}

void SLN::TimedRequest(void* void_sln)
{
	SLN* sln(static_cast<SLN*>(void_sln));

	sln->Request();
}
