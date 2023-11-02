#pragma once

#include "macros.h"
#include <cstdint>
#include <string>
#include <vector>

using size_t = std::size_t;

struct _ENetPacket;

class Packet {
public:
	/// Empty, preallocated container
	Packet(size_t n_prealloc = 200);
	/// Initialization by data copy
	Packet(const void *bytes, size_t len);
	/// Take ownership of an existing packet (from ENet)
	Packet(_ENetPacket **pkt);
	/// No-copy Packet clone with separate read/write tracking
	/// Can be used to read specific sections or overwrite placeholders
	Packet(Packet *pkt);

	~Packet();
	DISABLE_COPY(Packet);
	Packet(Packet &&) = default;

	void setBigEndian(bool b = true) { m_is_big_endian = b; }
	inline size_t getReadPos() const { return m_read_offset; };
	// Amount of bytes left over for reading
	inline size_t getRemainingBytes() { return size() - m_read_offset; }
	/// Artificial read limit e.g. for decompression
	void limitRemainingBytes(size_t n);

	inline size_t size() const { return m_write_offset; }
	const void *data() const;

	std::string dump(size_t n = 10);

	// For network sending
	_ENetPacket *ptr();

	template<typename T>
	T read();

	template<typename T>
	inline void read(T &v) { v = read<T>(); }

	template<typename T>
	void write(T v);

	std::string readStr16();
	void writeStr16(const std::string &str);

	// Preallocated additional bytes for large data writes
	void ensureCapacity(size_t nbytes);

	// For data (de)compression
	void writeRaw(const uint8_t *data, size_t nbytes);

	// ========== Optimization tricks ==========
	/// Read maximal N bytes without copying bytes
	size_t readRawNoCopy(const uint8_t **data, size_t nbytes);
	/// Move cursor back N bytes that were not read
	void   readRawNoCopyEnd(size_t nbytes);
	/// Reserve space for writing without moving the cursor
	uint8_t *writePreallocStart(size_t n_reserve);
	/// Move forth N bytes that were written to
	void     writePreallocEnd(size_t nbytes);

	uint16_t data_version = 0;

private:
	inline void checkLength(size_t nbytes);

	bool m_is_big_endian = false;
	size_t m_read_offset = 0;
	size_t m_write_offset = 0;
	_ENetPacket *m_data = nullptr;
};

// In sync with client_packethandler.cpp
enum class Packet2Client : uint16_t {
	Quack = 0,
	Hello,
	Message,
	Auth,
	Lobby,
	WorldData, // initialization or reset
	Join,
	Leave,
	SetPosition, // generally respawn
	Move,
	Chat,
	PlaceBlock,
	Key, // key blocks
	GodMode,
	Smiley,
	PlayerFlags,
	WorldMeta,
	MAX_END
};

// In sync with server_packethandler.cpp
enum class Packet2Server : uint16_t {
	Quack = 0,
	Hello,
	Auth,
	GetLobby,
	Join,
	Leave,
	Move,
	Chat,
	PlaceBlock,
	TriggerBlocks, // key/kill ?
	GodMode,
	Smiley,
	MAX_END
};
