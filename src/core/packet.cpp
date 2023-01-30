#include "packet.h"

#include <stdexcept>
#include <string.h> // memcpy

Packet::Packet(const char *data, size_t len)
{
	m_data = std::vector<uint8_t>(len);
	memcpy(&m_data[0], data, len);
}

// -------------- Public members -------------

template <typename T>
T Packet::read()
{
	checkLength(sizeof(T));

	size_t offset = m_read_offset;
	m_read_offset += sizeof(T);
	return *(T *)&m_data[offset];
}

template <typename T>
void Packet::write(T v)
{
	ensureCapacity(sizeof(T));

	T *dst = (T *)&m_data[m_write_offset];
	*dst = v;

	m_write_offset += sizeof(T);
}

#define DEFINE_PACKET_TYPES(TYPE) \
	template TYPE Packet::read<TYPE>(); \
	template void Packet::write<TYPE>(TYPE);

DEFINE_PACKET_TYPES(uint8_t)
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

	std::string str((const char *)&m_data[offset], len);
	return str;
}

void Packet::writeStr16(const std::string &str)
{
	if (str.size() > UINT16_MAX)
		throw std::out_of_range("String too long");

	ensureCapacity(2 + str.size());

	write<uint16_t>(str.size());
	memcpy(&m_data[m_write_offset], str.c_str(), str.size());

	m_write_offset += str.size();
}

// -------------- Private members -------------

void Packet::ensureCapacity(size_t nbytes)
{
	if (m_write_offset + nbytes > m_data.capacity())
		m_data.resize(m_write_offset + nbytes);
}

void Packet::checkLength(size_t nbytes)
{
	if (m_read_offset + nbytes > m_data.size())
		throw std::out_of_range("Packet has no leftover data");
}
