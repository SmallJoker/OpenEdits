#include "blockparams.h"
#include "packet.h"
#include <string.h> // memset

BlockParams::BlockParams(BlockParams::Type type)
{
	if ((int)type >= (int)Type::INVALID)
		throw std::runtime_error("Unknown BlockParams Type: " + std::to_string((int)type));

	// Just in case
	memset((void *)this, 0, sizeof(BlockParams));
	m_type = type;

	switch (m_type) {
		case Type::INVALID:
		case Type::None:
			break;
		case Type::Text:
			text = new std::string();
			break;
		case Type::Gate:
		case Type::Teleporter:
			break;
	}
}

void BlockParams::reset()
{
	switch (m_type) {
		case Type::INVALID:
		case Type::None:
			break;
		case Type::Text:
			delete text;
			break;
		case Type::Gate:
		case Type::Teleporter:
			break;
	}
}

BlockParams::~BlockParams()
{
	reset();
}

BlockParams::BlockParams(const BlockParams &other) :
	m_type(Type::None)
{
	*this = other;
}

BlockParams &BlockParams::operator=(const BlockParams &other)
{
	reset();

	memcpy((void *)this, &other, sizeof(BlockParams));

	switch (m_type) {
		case Type::INVALID:
		case Type::None:
			break;
		case Type::Text:
			text = new std::string(*other.text);
			break;
		case Type::Gate:
		case Type::Teleporter:
			break;
	}

	return *this;
}


void BlockParams::read(Packet &pkt)
{
	// In case of new types: change "m_type" after reading
	switch (m_type) {
		case Type::INVALID:
		case Type::None:
			break;
		case Type::Text:
			*text = pkt.readStr16();
			break;
		case Type::Gate:
			pkt.read(gate.value);
			break;
		case Type::Teleporter:
			pkt.read(teleporter.visual);
			pkt.read(teleporter.id);
			pkt.read(teleporter.dst_id);
			break;
	}
}

void BlockParams::write(Packet &pkt) const
{
	switch (m_type) {
		case Type::INVALID:
		case Type::None:
			break;
		case Type::Text:
			pkt.writeStr16(*text);
			break;
		case Type::Gate:
			pkt.write(gate.value);
			break;
		case Type::Teleporter:
			pkt.write(teleporter.visual);
			pkt.write(teleporter.id);
			pkt.write(teleporter.dst_id);
			break;
	}
}
