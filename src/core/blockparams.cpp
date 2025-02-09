#include "blockparams.h"
#include "packet.h"
#include <stdexcept> // runtime_error
#include <string.h> // memset

BlockParams::BlockParams(BlockParams::Type type)
{
	set(type);
}

void BlockParams::set(BlockParams::Type type)
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
		case Type::STR16:
			text = new std::string();
			break;
		case Type::U8:
		case Type::U8U8U8:
			break;
	}
}

void BlockParams::reset()
{
	switch (m_type) {
		case Type::INVALID:
		case Type::None:
			break;
		case Type::STR16:
			delete text;
			break;
		case Type::U8:
		case Type::U8U8U8:
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
	if (m_type != other.m_type) {
		reset();
		set(other.m_type);
	}

	// Not very efficient but needs less code maintenance
	Packet pkt;
	other.write(pkt);
	read(pkt);

	return *this;
}

bool BlockParams::operator==(const BlockParams &other) const
{
	switch (m_type) {
		case Type::INVALID:
		case Type::None:
			return true;
		case Type::STR16:
			return *text == *other.text;
		case Type::U8:
			return param_u8 == other.param_u8;
		case Type::Teleporter:
			return teleporter.rotation == other.teleporter.rotation
				&& teleporter.id == other.teleporter.id
				&& teleporter.dst_id == other.teleporter.dst_id;
	}
	return false; // ERROR
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
		case Type::U8:
			pkt.read(param_u8);
			break;
		case Type::Teleporter:
			pkt.read(teleporter.rotation);
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
		case Type::U8:
			pkt.write(param_u8);
			break;
		case Type::Teleporter:
			pkt.write(teleporter.rotation);
			pkt.write(teleporter.id);
			pkt.write(teleporter.dst_id);
			break;
	}
}
