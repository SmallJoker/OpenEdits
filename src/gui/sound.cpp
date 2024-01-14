#include "sound.h"
#include <stdio.h>

const core::vector2df SoundSpec::POS_NONE(INFINITY, INFINITY);

#ifdef HAVE_SOUND

#include <AL/al.h>
#include <AL/alc.h>
#include <fstream>
#include <map>
#include <vector>

#define MINIMP3_ONLY_MP3
#define MINIMP3_NONSTANDARD_BUT_LOGICAL
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"


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
	loaded_sounds.clear();
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
	alListener3f(AL_POSITION, pos.X, pos.Y, -20.0f); // reduces stereo effect
}

void SoundPlayer::play(const SoundSpec &spec)
{
	auto *file = getOrLoad(spec.name);
	if (!file)
		return;

	SoundSource &src = m_sources.emplace_back();

	// http://openal.org/documentation/OpenAL_Programmers_Guide.pdf
	alSourcei (src.source, AL_BUFFER, file->buffer);
	bool positional = (spec.pos != SoundSpec::POS_NONE);
	if (positional) {
		alSource3f(src.source, AL_POSITION, spec.pos.X, spec.pos.Y, 0);
		alSourcef (src.source, AL_GAIN, 6.0f * spec.gain);
	} else {
		alSourcei(src.source, AL_SOURCE_RELATIVE, true);
		alSourcef (src.source, AL_GAIN, 0.6f * spec.gain);
	}
	alSourcef(src.source, AL_PITCH, spec.pitch);

	alSourcePlay(src.source);

	if (m_do_log) {
		static const char *modes[2] = { "YES", "NO" };
		printf("SoundPlayer::play name='%s', pitch=%.1f, pos=%s\n",
			spec.name, spec.pitch, modes[positional]
		);
	}
}

const SoundFile *SoundPlayer::getOrLoad(const char *name)
{
	auto it = loaded_sounds.find(name);
	if (it != loaded_sounds.end())
		return &it->second;

	enum FileFormat {
		FF_RAW_MONO_S16,
		FF_MP3,
		FF_INVALID
	};
	static const char *EXTENSIONS[FF_INVALID] = {
		".mono_s16.raw",
		".mp3"
	};

	std::ifstream is;
	unsigned file_format;
	std::string file_path_1 = "assets/sounds/";
	file_path_1.append(name);
	std::string file_path;

	for (file_format = 0; file_format < FF_INVALID; ++file_format) {
		file_path = file_path_1 + EXTENSIONS[file_format];
		is = std::ifstream(file_path, std::ios_base::binary);
		if (is.good())
			break;
	}

	ALuint al_format = AL_FORMAT_MONO16;
	ALuint al_samplerate = 44100;
	std::vector<char> data;
	size_t data_size = 0;

	switch ((FileFormat)file_format) {
	case FF_RAW_MONO_S16:
		{
			// 44100 Hz, mono, int16_t, little endian

			is.seekg(std::ios_base::end);
			data.resize((size_t)is.tellg()); // ::ate
			is.seekg(0);

			is.read(&data[0], data.size());
			data_size = is.gcount();
			if (data_size != data.size())
				fprintf(stderr, "SoundPlayer: Read mismatch\n");
		}
		break;
	case FF_MP3:
		{
			is.close();

			int status;
			mp3dec_ex_t dec;
			const size_t MIN_SIZE = MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(mp3d_sample_t);

			if ((status = mp3dec_ex_open(&dec, file_path.c_str(), MP3D_SEEK_TO_SAMPLE))) {
				fprintf(stderr, "SoundPlayer: failed to open MP3. status=%d\n", status);
				goto mp3_exit;
			}

			al_samplerate = dec.info.hz;
			switch (dec.info.channels) {
				case 1: al_format = AL_FORMAT_MONO16; break;
				case 2: al_format = AL_FORMAT_STEREO16; break;
				default:
					fprintf(stderr, "SoundPlayer: cannot playback MP3 channel count=%d\n", dec.info.channels);
					goto mp3_exit;
			}

			data.resize(4 * MIN_SIZE);

			while (true) {
				if (data_size + MIN_SIZE > data.size())
					data.resize(data.size() * 4);

				// Function outputs int16_t data. OpenAL needs int16_t data. Works for me.
				size_t space = data.size() - data_size;
				size_t samples = mp3dec_ex_read(&dec, (mp3d_sample_t *)&data[data_size], space / sizeof(mp3d_sample_t));

				data_size += samples * sizeof(mp3d_sample_t);
				if (samples == 0)
					break;
			}

mp3_exit:
			mp3dec_ex_close(&dec);
		}
		break;
	default:
		fprintf(stderr, "SoundPlayer: cannot find sound named '%s'\n", name);
		return nullptr;
	}

	if (data_size == 0)
		return nullptr;

	// Copy the data to OpenAL
	SoundFile &sf = loaded_sounds[name];
	alBufferData(sf.buffer, al_format, &data[0], data_size, al_samplerate);
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

void SoundPlayer::play(const SoundSpec &spec)
{
	if (m_do_log)
		puts("SoundPlayer: demo sound!");
}

#endif // HAVE_SOUND
