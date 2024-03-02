#include "unittest_internal.h"


#ifdef HAVE_LUA

#include "core/connection.h" // PROTOCOL_VERSION_*
#include "core/script.h"
#include "server/remoteplayer.h"


void unittest_script()
{
	Script script;
	script.init();
	script.loadFromFile("assets/scripts/main.lua");

	RemotePlayer p(12345, PROTOCOL_VERSION_MAX);
	CHECK(script.loadDefinition(666));
	//script.whileIntersecting(&p);

}


#else

void unittest_script()
{
	puts("Script not available");
}

#endif

