#pragma once

#ifndef _OV_FILE_H_
#include "XiphTypes.h"
#endif
#include "aes.h"

#include <inttypes.h>
#include <vector>

class VorbisEncrypter
{
public:
	// Construct an encrypter using the given unencrypted file as a source.
	VorbisEncrypter(void* datasource, ov_callbacks cbStruct);
	// Construct an encrypter using the given plain ogg vorbis file as a source.
	VorbisEncrypter(void* datasource, int oggMapType, ov_callbacks cbStruct);
	~VorbisEncrypter();

	// Read encrypted Mogg data. Returns number of elements read.
	size_t ReadRaw(void* buf, size_t elementSize, size_t elements);
	size_t TellRaw() const { return position; }
	size_t LengthRaw() const { return encrypted_length; };
	// Only set if a plain ogg vorbis file was used
	uint32_t sample_rate;

private:
	void GenerateIv(uint8_t* header_ptr);
	void FixCounter(size_t decryptedPos);
	void EncryptBytes(uint8_t* buffer, size_t offset, size_t count);

	ov_callbacks cb_struct{};
	void* file_ref{ 0 };

	size_t position{ 0 };
	size_t encrypted_length{ 0 };
	std::vector<uint8_t> hmx_header;

	size_t source_ogg_offset{ 0 };
	aes_ctr_128* initial_counter{ 0 };
	aes_ctr_128 counter{ 0 };
};
