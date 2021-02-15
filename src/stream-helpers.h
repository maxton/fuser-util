#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

// Read little-endian value
template<typename T>
T read(std::istream& stream) {
  T ret;
  stream.read((char*)&ret, sizeof(T));
  return ret;
}

// Read big-endian value
template<typename T>
T read_be(std::istream& stream) {
  uint8_t bytes[sizeof(T)];
  stream.read((char*)bytes, sizeof(T));
  T ret;
  uint8_t *p = (uint8_t*)&ret;
  for (int i = 1; i <= sizeof(T); i++) {
    p[i - 1] = bytes[sizeof(T) - i];
  }
  return ret;
}
template<> uint8_t read_be(std::istream& stream);

// Read big-endian 24 bit integer
uint32_t read_i24_be(std::istream& stream);

// Read midi multi-byte
uint32_t read_mb(std::istream& stream);
// Read fixed-length string
std::string read_str(std::istream& stream, uint32_t length);
// Read length-prefixed string
std::string read_symbol(std::istream& stream);
// Read length-prefixed + null-terminated string
std::string read_ue4text(std::istream& stream);

template<typename T>
void read_array(std::istream& stream, T* data, size_t count, std::function<T(std::istream&)> read_func) {
  for(auto i = 0u; i < count; i++) {
    data[i] = read_func(stream);
  }
}

template<typename T>
void read_vector(std::istream& stream, std::vector<T>& array, std::function<T(std::istream&)> read_func) {
  uint32_t array_size = read<uint32_t>(stream);
  for(auto i = 0u; i < array_size; i++) {
    array.push_back(read_func(stream));
  }
}

template<typename T>
void write(std::ostream& stream, const T& val) {
  stream.write((char*)&val, sizeof(T));
}

template<typename T>
void write_be(std::ostream& stream, const T& val) {
  char* val_alias = (char*)&val;
  for(int i = 1; i <= sizeof(T); i++) {
    stream.put(val_alias[sizeof(T) - i]);
  }
}

// Writes a MIDI multi-byte value to the stream.
void write_mb(std::ostream& stream, const uint32_t& value);
// Writes a 24-bit integer (big-endian) to the stream
void write_i24_be(std::ostream& stream, const uint32_t& value);
// Writes a string with no length prefix.
void write_str(std::ostream& stream, const std::string& string);
// Writes a length prefixed string.
void write_symbol(std::ostream& stream, const std::string& symbol);

template<typename T>
void write_array(std::ostream& stream, const T* data, size_t count, std::function<void(std::ostream&, const T&)> write_func) {
  for(auto i = 0u; i < count; i++) {
    write_func(stream, data[i]);
  }
}

template<typename T>
void write_vector(std::ostream& stream, const std::vector<T>& vector, std::function<void(std::ostream&, const T&)> write_func) {
  write<uint32_t>(stream, vector.size());
  for(const auto& el : vector) {
    write_func(stream, el);
  }
}