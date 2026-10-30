#pragma once

#include "packet.h"
#include <iosfwd>

Packet packet_from_istream(std::istream &is);
