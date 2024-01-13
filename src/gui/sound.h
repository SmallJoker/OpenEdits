#pragma once

#include <list>
#include <stddef.h> // size_t
#include <vector2d.h>

struct ALCcontext_struct;
typedef struct ALCcontext_struct ALCcontext;

struct ALCdevice_struct;
typedef struct ALCdevice_struct ALCdevice;

struct SoundFile;
struct SoundSource;

using namespace irr;

class SoundPlayer {
public:
	SoundPlayer(bool do_log);
	~SoundPlayer();

	void step();

	// When the camera moves
	void updateListener(core::vector2df pos);

	void play(const char *name, core::vector2df pos, float pitch = 1);

private:
	const SoundFile *getOrLoad(const char *name);

	bool m_do_log = false;

	ALCdevice *m_device;
	ALCcontext *m_context;

	std::list<SoundSource> m_sources;
};
