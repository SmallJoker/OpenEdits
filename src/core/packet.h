#pragma once

#include <cstdint>
#include <string>
#include <vector>

using size_t = std::size_t;

class Packet {
public:
	Packet() {}
	Packet(const char *data, size_t len);

	size_t getRemainingBytes()
	{ return m_data.size() - m_read_offset; }

	template<typename T>
	T read();

	template<typename T>
	void write(T v);

	std::string readStr16();
	void writeStr16(const std::string &str);

private:
	inline void ensureCapacity(size_t nbytes);
	inline void checkLength(size_t nbytes);

	size_t m_read_offset = 0;
	size_t m_write_offset = 0;
	std::vector<uint8_t> m_data;
};
