#pragma once

#include "macros.h"
#include <cstdint>
#include <string>
#include <vector>

using size_t = std::size_t;

struct _ENetPacket;

class Packet {
public:
	Packet();
	Packet(const char *data, size_t len);
	Packet(_ENetPacket **pkt); // takes ownership
	~Packet();
	DISABLE_COPY(Packet);

	size_t getRemainingBytes() { return size() - m_read_offset; }
	inline size_t size() const { return m_write_offset; }

	std::string dump(size_t n = 10);

	// For network sending
	_ENetPacket *data();

	template<typename T>
	T read();

	template<typename T>
	void write(T v);

	std::string readStr16();
	void writeStr16(const std::string &str);

private:
	inline void ensureCapacity(size_t nbytes);
	inline void checkLength(size_t nbytes);

	size_t m_read_offset = 0;
	size_t m_write_offset = 0;
	_ENetPacket *m_data = nullptr;
};

// In sync with client_packethandler.cpp
enum class Packet2Client : uint16_t {
	Quack = 0,
	Hello,
	Error,
	Join,
	Leave,
	Move
};

// In sync with server_packethandler.cpp
enum class Packet2Server : uint16_t {
	Quack = 0,
	Hello,
	Join,
	Leave,
	Move
};
