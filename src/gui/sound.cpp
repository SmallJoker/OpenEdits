#include "sound.h"
#include <stdio.h>

#ifdef HAVE_SOUND

#include <AL/al.h>
#include <AL/alc.h>
#include <fstream>
#include <map>
#include <vector>

struct SoundFile {
	SoundFile()
	{
		alGenBuffers(1, &buffer);
	}

	~SoundFile()
	{
		alDeleteBuffers(1, &buffer);
	}

	ALuint buffer;
};

static std::map<std::string, SoundFile> loaded_sounds;

struct SoundSource {
	SoundSource()
	{
		alGenSources(1, &source);
		ALenum err = alGetError();
		if (err != AL_NO_ERROR) {
			source = 0;
			fprintf(stderr, "SoundSource: alGenSources error %d\n", err);
			return;
		}

		alSourcef(source, AL_PITCH, 1);
		alSourcef(source, AL_GAIN, 1);
		alSourcei(source, AL_LOOPING, AL_FALSE);
	}

	~SoundSource()
	{
		if (source)
			alDeleteSources(1, &source);
	}

	bool isAlive()
	{
		ALenum state;
		alGetSourcei(source, AL_SOURCE_STATE, &state);
		return state == AL_PLAYING;
	}

	ALuint source;
};

SoundPlayer::SoundPlayer(bool do_log) :
	m_do_log(do_log)
{
	m_device = alcOpenDevice(nullptr);

	m_context = alcCreateContext(m_device, NULL);
	if (!alcMakeContextCurrent(m_context)) {
		m_context = nullptr;
		puts("SoundPlayer: error creating context");
	}

	puts("--> SoundPlayer start");
}

SoundPlayer::~SoundPlayer()
{
	if (m_context) {
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(m_context);
	}
	if (m_device)
		alcCloseDevice(m_device);

	puts("<-- SoundPlayer stop");
}

void SoundPlayer::step()
{
	for (auto it = m_sources.begin(); it != m_sources.end(); ) {
		if (!it->isAlive()) {
			if (m_do_log)
				printf("SoundPlayer: source %i is done\n", it->source);
			it = m_sources.erase(it);
		} else {
			++it;
		}
	}
}

void SoundPlayer::updateListener(core::vector2df pos)
{
	alListener3f(AL_POSITION, pos.X, pos.Y, -10.0f); // reduces stereo effect
}

void SoundPlayer::play(const char *name, core::vector2df pos, float pitch)
{
	auto *file = getOrLoad(name);
	if (!file)
		return;

	SoundSource &src = m_sources.emplace_back();

	// http://openal.org/documentation/OpenAL_Programmers_Guide.pdf
	alSourcei (src.source, AL_BUFFER, file->buffer);
	alSource3f(src.source, AL_POSITION, pos.X, pos.Y, 0);
	alSourcef (src.source, AL_GAIN, 4.0f);
	alSourcef (src.source, AL_PITCH, pitch);

	alSourcePlay(src.source);

	if (m_do_log)
		printf("SoundPlayer::play name='%s', pitch=%.1f\n", name, pitch);
}

const SoundFile *SoundPlayer::getOrLoad(const char *name)
{
	auto it = loaded_sounds.find(name);
	if (it != loaded_sounds.end())
		return &it->second;

	// Only supports signed 16-bit, RAW for now
	std::ifstream is(std::string("assets/sounds/") + name,
		// Ate all the bytes :(
		std::ios_base::binary | std::ios_base::ate
	);

	if (!is.good()) {
		fprintf(stderr, "SoundPlayer: cannot find sound named '%s'\n", name);
		return nullptr;
	}

	ALuint format = AL_FORMAT_MONO16;
	ALuint samplerate = 44100;

	std::vector<char> data;
	data.resize((size_t)is.tellg()); // ::ate

	is.seekg(0);
	is.read(&data[0], data.size());
	if ((size_t)is.gcount() != data.size())
		fprintf(stderr, "SoundPlayer: Read mismatch\n");


	// Copy the data to OpenAL
	SoundFile &sf = loaded_sounds[name];
	alBufferData(sf.buffer, format, &data[0], data.size(), samplerate);
	return &sf;
}


#else // HAVE_SOUND

struct SoundSource {};

SoundPlayer::SoundPlayer(bool do_log) :
	m_do_log(do_log)
{
	fputs("SoundPlayer not available!\n", stderr);
}

SoundPlayer::~SoundPlayer() {}

void SoundPlayer::step() {}

void SoundPlayer::updateListener(core::vector2df pos) {}

void SoundPlayer::play(const char *name, core::vector2df pos, float pitch)
{
	if (m_do_log)
		puts("SoundPlayer: demo sound!");
}

#endif // HAVE_SOUND
