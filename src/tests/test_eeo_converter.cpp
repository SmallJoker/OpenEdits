#include "unittest_internal.h"
#include "core/world.h"
#include "server/eeo_converter.h"

void unittest_eeo_converter()
{
	//EEOconverter::inflate("../8s9kvu.eelvl");

	World output("eeo_import");
	EEOconverter conv(output);
	conv.import("../8s9kvu.eelvl");

	auto meta = output.getMeta();

	throw std::runtime_error("Stop");
}
