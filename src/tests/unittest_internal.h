#pragma once

#include <stdexcept>

#define CHECK(cond) \
	if (!(cond)) { \
		char buf[100]; \
		snprintf(buf, sizeof(buf), "Unittest fail: ( %s ) @ L %i", #cond, __LINE__); \
		throw std::runtime_error(buf); \
	}

class BlockManager;
extern BlockManager *g_blockmanager;

void unittest_tic();
void unittest_toc(const char *name);
