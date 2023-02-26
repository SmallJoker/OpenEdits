#pragma once

#include <cstdint>
#include <string>

class Packet;

struct BlockParams {
	// Order must not be changed: is saved to disk
	enum class Type : uint8_t {
		None,
		Text,
		Gate,
		Teleporter,
		// New parameters need a new type
		INVALID
	};

	BlockParams(Type type = Type::None);
	~BlockParams();

	Type getType() const { return m_type; }
	bool operator ==(Type type) const { return m_type == type; }
	bool operator !=(Type type) const { return m_type != type; }

	// Copy
	BlockParams(const BlockParams &other);
	BlockParams &operator=(const BlockParams &other);

	void read(Packet &pkt);
	void write(Packet &pkt) const;

	union {
		std::string *text;
		struct {
			uint8_t value;
		} gate;
		struct {
			uint8_t visual; // e.g. rotation
			uint8_t id;
			uint8_t dst_id;
		} teleporter;
	};

private:
	void reset();

	Type m_type;
};
