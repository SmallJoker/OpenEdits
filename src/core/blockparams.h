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

	/* Approach 1: Use an array to be more generic
		u32 types = 0xDDCCBBAA;
		union {
			...
		} values[4];
		+ Direct indexing possible (useful for rending callbacks optimizations)
		- Slow hashing and copying
		- Wasteful in memory (4 + 4 * 8 = 36 bytes)
	*/

	/* Approach 2: Reuse `Packet` and keep only the payload:
		u32 types = 0xDDCCBBAA;
		union {
			struct { // length <= 7
				/// If divisible by 4 or 8: use `data_long`, else `data_short`
				/// https://en.cppreference.com/w/c/language/object.html#Alignment
				u8 alignment_detector;
				u8 data_short[7]; // requires ((alignment_detector & 0x3) != 0)
			}
			u8 *data_long; // length > 7
		};
		+ Fast hashing and copying
		+ Compact in memory (12 bytes)
		- No direct indexing
	*/

	/* Approach 3: Array/linked list hybrid
		u32 types = 0xDDCCBBAA;
		union Value {
			uint8_t param_u8;
			char *str;
			Data *next; // 0xDDCCBB part of "types" if needed
		} values[2];
		+ Direct indexing possible
		- Slow hashing and copying
		o Neutral in memory usage (20 bytes)
	*/

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
