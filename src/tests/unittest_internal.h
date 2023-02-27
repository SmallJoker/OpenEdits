#pragma once

#include <stdexcept>

#define CHECK(cond) \
	if (!(cond)) { \
		std::string v("Unittest fail: ( " #cond " ) @ L"); \
		v.append(std::to_string(__LINE__)); \
		throw std::runtime_error(v); \
	}

class BlockManager;
extern BlockManager *g_blockmanager;
