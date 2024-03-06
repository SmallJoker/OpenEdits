#include "unittest_internal.h"


#ifdef HAVE_LUA

#include "core/blockmanager.h"
#include "core/connection.h" // PROTOCOL_VERSION_*
#include "core/script.h"
#include "server/remoteplayer.h"


void unittest_script()
{
	BlockManager bmgr;
	Script script(&bmgr);
	script.init();
	CHECK(script.loadFromFile("assets/scripts/main.lua"));

	RemotePlayer p(12345, PROTOCOL_VERSION_MAX);
	script.setPlayer(&p);

	Script::IntersectionData id;
	id.block_id = 12;
	script.whileIntersecting(id);

}


#else

void unittest_script()
{
	puts("Script not available");
}

#endif

