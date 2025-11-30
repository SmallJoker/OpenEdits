#pragma once

#include <cstdint>
#include <string>

class Packet;


struct BlockParams {
	// Order must not be changed: is saved to disk
	enum class Type : uint8_t {
		None = 0,
		STR16 = 1,
		Text = 1,

		U8 = 2,

		U8U8U8 = 3,
		Teleporter = 3,
		// New parameters need a new type
		INVALID
	};

	BlockParams(Type type = Type::None);
	~BlockParams();

	Type getType() const { return m_type; }
	bool operator ==(Type type) const { return m_type == type; }
	bool operator !=(Type type) const { return m_type != type; }
	bool operator ==(const BlockParams &other) const;

	// Copy
	BlockParams(const BlockParams &other);
	BlockParams &operator=(const BlockParams &other);

	void read(Packet &pkt);
	void write(Packet &pkt) const;

	// TODO: Implementation in src/script/xxx.cpp
	void pushToLua();
	void readFromLua();

	// Approach 1: Use an array to be more generic
	// u32 types = 0xDDCCBBAA;
	// Entry data[4];

	// Approach 2: Reuse `Packet` and keep only the payload:
	// u16 length;
	// u8[??] data_from_lua;

	union {
		std::string *text;
		uint8_t param_u8;
		struct {
			uint8_t rotation;
			uint8_t id;
			uint8_t dst_id;
		} teleporter;
		uint32_t param_u32;
	};

private:
	void set(Type type);
	void reset();

	Type m_type;
};
