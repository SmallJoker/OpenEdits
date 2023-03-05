#include "unittest_internal.h"
#include "core/world.h"
#include "server/eeo_converter.h"

void unittest_eeo_converter()
{
#ifndef HAVE_ZLIB
	puts("EEOconverter is not available");
#else

	return;
	// TODO: Add actual test

	//EEOconverter::inflate("../8s9kvu.eelvl");

	World output(g_blockmanager, "eeo_import");
	EEOconverter conv(output);
	conv.fromFile("8s9kvu.eelvl");

	//auto &meta = output.getMeta();

#endif
}
