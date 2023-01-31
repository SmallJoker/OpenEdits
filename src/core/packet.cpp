#include "packet.h"

#include <enet/enet.h>
#include <stdexcept>
#include <string.h> // memcpy

Packet::Packet()
{
	m_data = enet_packet_create(nullptr, 200, 0);
}

Packet::Packet(const char *data, size_t len)
{
	m_data = enet_packet_create(data, len, 0);
	m_write_offset = len;
}

Packet::Packet(_ENetPacket **pkt)
{
	m_data = *pkt;
	m_write_offset = m_data->dataLength;

	*pkt = nullptr;
}

Packet::~Packet()
{
	enet_packet_destroy(m_data);
}


// -------------- Public members -------------

_ENetPacket *Packet::data()
{
	m_data->dataLength = m_write_offset;
	return m_data;
}


template <typename T>
T Packet::read()
{
	checkLength(sizeof(T));

	T ret;
	memcpy(&ret, &m_data->data[m_read_offset], sizeof(T));
	m_read_offset += sizeof(T);
	return ret;
}

template <typename T>
void Packet::write(T v)
{
	ensureCapacity(sizeof(T));

	memcpy(&m_data->data[m_write_offset], &v, sizeof(T));
	m_write_offset += sizeof(T);
}

#define DEFINE_PACKET_TYPES(TYPE) \
	template TYPE Packet::read<TYPE>(); \
	template void Packet::write<TYPE>(TYPE);

DEFINE_PACKET_TYPES(uint8_t)
DEFINE_PACKET_TYPES(int16_t)
DEFINE_PACKET_TYPES(uint16_t)
DEFINE_PACKET_TYPES(int32_t)
DEFINE_PACKET_TYPES(uint32_t)
DEFINE_PACKET_TYPES(float)

#undef DEFINE_PACKET_TYPES

std::string Packet::readStr16()
{
	uint16_t len = read<uint16_t>();
	checkLength(len);
	size_t offset = m_read_offset;
	m_read_offset += len;

	std::string str((const char *)&m_data->data[offset], len);
	return str;
}

void Packet::writeStr16(const std::string &str)
{
	if (str.size() > UINT16_MAX)
		throw std::out_of_range("String too long");

	ensureCapacity(2 + str.size());

	write<uint16_t>(str.size());
	memcpy(&m_data->data[m_write_offset], str.c_str(), str.size());

	m_write_offset += str.size();
}

// -------------- Private members -------------

void Packet::ensureCapacity(size_t nbytes)
{
	if (m_write_offset + nbytes > size())
		enet_packet_resize(m_data, (m_write_offset + nbytes) * 2);
}

void Packet::checkLength(size_t nbytes)
{
	if (m_read_offset + nbytes > size())
		throw std::out_of_range("Packet has no leftover data");
}
