#include "packet.h"
#include "network_enums.h"

#include <enet/enet.h>
#include <stdexcept>
#include <string.h> // memcpy
#include <sstream>

// We cannot be sure enough...
static_assert(sizeof(Packet2Client) == 2, "");
static_assert(sizeof(Packet2Server) == 2, "");

Packet::Packet(size_t n_prealloc)
{
	m_data = enet_packet_create(nullptr, n_prealloc, 0);
	m_data->referenceCount++; // until ~Packet
}

Packet::Packet(const void *bytes, size_t len)
{
	m_data = enet_packet_create(bytes, len, 0);
	m_data->referenceCount++; // until ~Packet

	m_write_offset = len;
}

Packet::Packet(_ENetPacket **pkt)
{
	m_data = *pkt;
	m_data->referenceCount++; // until ~Packet

	m_write_offset = m_data->dataLength;
	*pkt = nullptr;
}

Packet::Packet(Packet *pkt)
{
	m_data = pkt->m_data;
	m_data->referenceCount++; // until ~Packet

	m_read_offset = pkt->m_read_offset;
	m_write_offset = pkt->m_write_offset;
	data_version = pkt->data_version;
}


Packet::~Packet()
{
	if (m_data->referenceCount < 1) {
		fprintf(stderr, "Counting is difficult %s\n", dump().c_str());
		std::terminate();
	} else {
		m_data->referenceCount--;
	}

	if (m_data->referenceCount == 0)
		enet_packet_destroy(m_data);
}


// -------------- Public members -------------


void Packet::limitRemainingBytes(size_t n)
{
	if (n > getRemainingBytes())
		throw std::out_of_range("Read limit is outside of available data");

	m_write_offset = size() - n;
}


const uint8_t *Packet::data() const
{
	return m_data->data;
}


std::string Packet::dump(size_t n)
{
	n = std::min<size_t>(n, m_write_offset);

	std::stringstream ss;
	ss << "[len=" << size() <<" : ";

	char buf[4];
	for (size_t i = 0; i < n; i++) {
		sprintf(buf, "%02X ", m_data->data[i]);
		ss << buf;
		// make groups of 8
		if ((i & 0x07) == 0x07)
			ss << ". ";
	}
	ss << "| \"";
	for (size_t i = 0; i < n; i++) {
		char c = m_data->data[i];
		ss << (std::isalnum(c) ? c : '.');
	}
	ss << "\"]";

	return ss.str();
}


_ENetPacket *Packet::ptrForSend()
{
	m_data->dataLength = m_write_offset;
	return m_data;
}

inline void *swap_be_le(void *dst, const void *src, size_t n)
{
	for (size_t i = 0; i < n; ++i)
		((uint8_t *)dst)[i] = ((uint8_t *)src)[(n - 1) - i];

	return (uint8_t *)dst + n;
}

template <typename T>
T Packet::read()
{
	checkLength(sizeof(T));

	T ret;
	(m_is_big_endian ? swap_be_le : memcpy)(&ret, &m_data->data[m_read_offset], sizeof(T));
	m_read_offset += sizeof(T);
	return ret;
}

template <typename T>
void Packet::write(T v)
{
	ensureCapacity(sizeof(T));

	(m_is_big_endian ? swap_be_le : memcpy)(&m_data->data[m_write_offset], &v, sizeof(T));
	m_write_offset += sizeof(T);
}

void Packet::readRaw(uint8_t *dst, size_t nbytes)
{
	checkLength(nbytes);
	memcpy(dst, &m_data->data[m_read_offset], nbytes);
	m_read_offset += nbytes;
}

void Packet::writeRaw(const uint8_t *data, size_t nbytes)
{
	if (nbytes == 0)
		return;

	ensureCapacity(nbytes);

	memcpy(&m_data->data[m_write_offset], data, nbytes);
	m_write_offset += nbytes;
}

size_t Packet::readRawNoCopy(const uint8_t **data, size_t nbytes)
{
	nbytes = std::min<size_t>(nbytes, getRemainingBytes());

	*data = &m_data->data[m_read_offset];
	m_read_offset += nbytes;
	return nbytes;
}

void Packet::readRawNoCopyEnd(size_t nbytes)
{
	if (m_read_offset < nbytes)
		throw std::out_of_range("Cannot move cursor back: packet too short");

	m_read_offset -= nbytes;
}

uint8_t *Packet::writePreallocStart(size_t n_reserve)
{
	ensureCapacity(n_reserve);
	return &m_data->data[m_write_offset];
}

void Packet::writePreallocEnd(size_t nbytes)
{
	if (m_write_offset + nbytes > m_data->dataLength)
		throw std::out_of_range("Cannot skip. Missing prealloc. Possible memory corruption!");

	m_write_offset += nbytes;
}

#define DEFINE_PACKET_TYPES(TYPE) \
	template TYPE Packet::read<TYPE>(); \
	template void Packet::write<TYPE>(TYPE);

DEFINE_PACKET_TYPES(uint8_t)
DEFINE_PACKET_TYPES(int16_t)
DEFINE_PACKET_TYPES(uint16_t)
DEFINE_PACKET_TYPES(int32_t)
DEFINE_PACKET_TYPES(uint32_t)
DEFINE_PACKET_TYPES(int64_t) // time_t
DEFINE_PACKET_TYPES(float)
DEFINE_PACKET_TYPES(Packet2Client)
DEFINE_PACKET_TYPES(Packet2Server)

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

void Packet::ensureCapacity(size_t nbytes)
{
	if (m_write_offset + nbytes > m_data->dataLength)
		enet_packet_resize(m_data, (m_write_offset + nbytes) * 2);
}

// -------------- Private members -------------

void Packet::checkLength(size_t nbytes)
{
	if (m_read_offset + nbytes > size())
		throw std::out_of_range("Packet has no leftover data");
}
