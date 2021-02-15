#include "stream-helpers.h"

// Specialization for byte
template<>
uint8_t read_be(std::istream& stream) {
  return (uint8_t)stream.get();
}

uint32_t read_i24_be(std::istream& stream) {
  uint8_t bytes[3];
  stream.read((char*)&bytes, 3);
  return bytes[2] | (bytes[1] << 8) | (bytes[0] << 16);
}

uint32_t read_mb(std::istream& stream) {
  uint32_t ret = 0;
  uint8_t b = read_be<uint8_t>(stream);
  ret += b & 0x7f;
  if (0x80 == (b & 0x80))
  {
    ret <<= 7;
    b = read_be<uint8_t>(stream);
    ret += b & 0x7f;
    if (0x80 == (b & 0x80))
    {
      ret <<= 7;
      b = read_be<uint8_t>(stream);
      ret += b & 0x7f;
      if (0x80 == (b & 0x80))
      {
        ret <<= 7;
        b = read_be<uint8_t>(stream);
        ret += b & 0x7f;
        if (0x80 == (b & 0x80))
          throw std::exception("Variable-length MIDI number > 4 bytes");
      }
    }
  }
  return ret;
}

std::string read_str(std::istream& stream, uint32_t length) {
  std::vector<char> chars(length);
  stream.read(chars.data(), length);
  return std::string(chars.data(), length);
}

std::string read_symbol(std::istream& stream) {
  return read_str(stream, read<uint32_t>(stream));
}

std::string read_ue4text(std::istream& stream) {
  auto str = read_str(stream, read<uint32_t>(stream) - 1);
  if (stream.get() != 0)
    throw std::exception("String was not null-terminated");
  return str;
}

// Writes a MIDI multi-byte value to the stream.
void write_mb(std::ostream& stream, const uint32_t& value)
{
  if (value > 0x7FU)
  {
    int max = 7;
    while ((value >> max) > 0x7FU) max += 7;
    while (max > 0)
    {
      stream.put((char)(((value >> max) & 0x7FU) | 0x80));
      max -= 7;
    }
  }
  stream.put((char)(value & 0x7FU));
}


void write_i24_be(std::ostream& stream, const uint32_t& value) {
  char* value_alias = (char*)&value;
  stream.put(value_alias[2]);
  stream.put(value_alias[1]);
  stream.put(value_alias[0]);
}

// Writes a string with no length prefix.
void write_str(std::ostream& stream, const std::string& string) {
  stream.write(string.c_str(), string.length());
}
// Writes a length prefixed string.
void write_symbol(std::ostream& stream, const std::string& symbol) {
  write<uint32_t>(stream, symbol.length());
  write_str(stream, symbol);
}