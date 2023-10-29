#pragma once

#include <cstdint>
#include <string>

class Packet;

struct BlockParams {
	// Order must not be changed: is saved to disk
	enum class Type : uint8_t {
		None,
		Text,
		U8,
		Teleporter,
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

	union {
		std::string *text;
		uint8_t param_u8;
		struct {
			uint8_t rotation;
			uint8_t id;
			uint8_t dst_id;
		} teleporter;
	};

private:
	void set(Type type);
	void reset();

	Type m_type;
};
