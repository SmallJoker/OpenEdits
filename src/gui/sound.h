#pragma once

#include <list>
#include <stddef.h> // size_t
#include <vector2d.h>

extern "C" {
#ifdef OPENAL_OLDER_THAN_1_21_0
	struct ALCcontext_struct;
	typedef struct ALCcontext_struct ALCcontext;

	struct ALCdevice_struct;
	typedef struct ALCdevice_struct ALCdevice;
#else
	/// If this throws an error: toggle the CMake option
	struct ALCcontext;
	struct ALCdevice;
#endif
}

struct SoundFile;
struct SoundSource;

using namespace irr;

struct SoundSpec {
	static const core::vector2df POS_NONE;

	SoundSpec(const char *name, core::vector2df pos = POS_NONE) :
		name(name), pos(pos) {}

	const char *name;
	core::vector2df pos;
	float pitch = 1.0f;
	float gain = 1.0f;
};

class SoundPlayer {
public:
	SoundPlayer(bool do_log);
	~SoundPlayer();

	/// Cleans up finished sound sources
	void step();

	/// Player position for positional sounds
	void updateListener(core::vector2df pos);

	void play(const SoundSpec &spec);

private:
	const SoundFile *getOrLoad(const char *name);

	bool m_do_log = false;

	ALCdevice *m_device;
	ALCcontext *m_context;

	std::list<SoundSource> m_sources;
};
