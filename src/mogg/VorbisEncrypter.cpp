#include "VorbisEncrypter.h"

#include <time.h>
#include "keys.h"
#include "OggMap.h"

typedef struct {
	uint32_t a;
	uint32_t b;
} MapEntry;

void VorbisEncrypter::GenerateIv(uint8_t* header_ptr) {
	// Generate IV
	srand(time(NULL));
	initial_counter = (aes_ctr_128*)header_ptr;
	// For convenience leave the low 4 IV bytes as zero
	for (int i = 0; i < 4; i++) {
		*header_ptr++ = 0;
	}
	// Generate the remaining 12 IV bytes with rand
	for (int i = 0; i < 12; i++) {
		*header_ptr++ = rand() & 0xFF;
	}
}

VorbisEncrypter::VorbisEncrypter(void* datasource, ov_callbacks cbStruct) 
	: file_ref(datasource), cb_struct(cbStruct) {
	cb_struct.seek_func(file_ref, 0, SEEK_END);
	uint32_t total_length = cbStruct.tell_func(file_ref);
	cb_struct.seek_func(file_ref, 0, SEEK_SET);
	struct {
		int version;
		int offset;
	} original_file_header;
	if (cbStruct.read_func(&original_file_header, sizeof(original_file_header), 1, file_ref) != 1)
		throw std::exception("Unable to read mogg header.");
	if (original_file_header.version != 0xA)
		throw std::exception("Source mogg must be version 10/0xA (unencrypted).");

	struct {
		uint32_t version;
		uint32_t chunk_size;
		uint32_t num_entries;
	} OggMapHdr;

	if (cbStruct.read_func(&OggMapHdr, sizeof(OggMapHdr), 1, file_ref) != 1)
		throw std::exception("Unable to read OggMap header.");
	size_t header_size = 16 /* IV */
		+ sizeof(original_file_header)
		+ sizeof(OggMapHdr)
		+ (OggMapHdr.num_entries * sizeof(MapEntry));
	hmx_header.resize(header_size);

	auto* header_ptr = hmx_header.data();
	auto new_header = original_file_header;
	new_header.version = 0xB;
	new_header.offset = header_size;
	// Copy Mogg header
	memcpy(header_ptr, &new_header, sizeof(new_header));
	header_ptr += sizeof(new_header);
	// Copy OggMap header
	memcpy(header_ptr, &OggMapHdr, sizeof(OggMapHdr));
	header_ptr += sizeof(OggMapHdr);
	// Copy OggMap data
	cb_struct.read_func(header_ptr, sizeof(MapEntry), OggMapHdr.num_entries, file_ref);
	header_ptr += sizeof(MapEntry) * OggMapHdr.num_entries;
	GenerateIv(header_ptr);
	encrypted_length = total_length - original_file_header.offset + hmx_header.size();
	source_ogg_offset = original_file_header.offset;
}


VorbisEncrypter::VorbisEncrypter(void* datasource, int oggMapType, ov_callbacks cbStruct)
	: file_ref(datasource), cb_struct(cbStruct)  {
	cb_struct.seek_func(file_ref, 0, SEEK_END);
	uint32_t total_length = cbStruct.tell_func(file_ref);
	cb_struct.seek_func(file_ref, 0, SEEK_SET);

	auto result = OggMap::Create(datasource, cbStruct);
	if (std::holds_alternative<std::string>(result)){
		throw std::exception(std::get<std::string>(result).c_str());
	}
	auto& map = std::get<OggMap>(result);
	auto mapData = map.Serialize();
	// 4 byte version, 4 byte offset, map, 16 byte IV
	hmx_header.resize(8 + mapData.size() + 16);
	auto *hdr_as_ints = (int32_t*)hmx_header.data();
	hdr_as_ints[0] = 0xB;
	hdr_as_ints[1] = (int32_t)hmx_header.size();
	memcpy(&hdr_as_ints[2], mapData.data(), mapData.size());
	GenerateIv(hmx_header.data() + hmx_header.size() - 16);

	source_ogg_offset = 0;
	encrypted_length = total_length + hmx_header.size();
	sample_rate = map.sample_rate;
}

VorbisEncrypter::~VorbisEncrypter() {
	cb_struct.close_func(file_ref);
}

size_t VorbisEncrypter::ReadRaw(void* buf, size_t elementSize, size_t elements)
{
	size_t count = elementSize * elements;
	size_t bytesRead = 0;
	size_t offset = 0;
	uint8_t* buffer = (uint8_t*)buf;

	if (position < hmx_header.size()) {
		while (position < hmx_header.size() && offset < count) {
			buffer[offset++] = hmx_header[position++];
			bytesRead++;
		}
	}
	count -= offset;

	if (position + count > encrypted_length) {
		count = encrypted_length - position;
	}
	if (count == 0) return 0;

	int32_t size_diff = (int32_t)source_ogg_offset - hmx_header.size();
	cb_struct.seek_func(file_ref, position + size_diff, SEEK_SET);
	size_t actualRead = cb_struct.read_func(buffer + offset, 1, count, file_ref);

	bytesRead += actualRead;
	position += actualRead;

	EncryptBytes(buffer, offset, actualRead);
	return bytesRead / elementSize;
}



// Fixes the counter
void VorbisEncrypter::FixCounter(size_t decryptedPos) {
	counter = *initial_counter;
	uint64_t low = counter.qwords[0];
	counter.qwords[0] += (decryptedPos >> 4);
	if (counter.qwords[0] < low) {
		counter.qwords[1]++;
	}
}

// Increments a 128-bit counter using 64-bit word size
inline void IncrementCounter(aes_ctr_128* ctr) {
	for (int ptr = 0; ptr < 2; ptr++) {
		ctr->qwords[ptr]++;
		if (ctr->qwords[ptr] != 0)
			break;
	}
}

/**
 * buffer: buffer to write into
 * offset: offset into buffer to start writing
 * count: number of bytes to write into buffer
 *
 * This relies on the internal state, specifically that oggPos is set to the
 * last READ location. So if oggPos is 30 and count is 10 it assumes that bytes from 20 to 30 are being decrypted.
 */
void VorbisEncrypter::EncryptBytes(uint8_t* buffer, size_t offset, size_t count)
{
	aes_ctr_128 cryptedCounter;
	size_t decryptedPos = position - count - hmx_header.size();
	int counterLoc = decryptedPos % 16;

	FixCounter(decryptedPos);
	AES128_ECB_encrypt(counter.bytes, ctrKey0B, cryptedCounter.bytes);
	for (int i = 0; i < count; i++)
	{
		if (decryptedPos != 0 && decryptedPos % 16 == 0 && i != 0)
		{
			FixCounter(decryptedPos);
			counterLoc = 0;
			AES128_ECB_encrypt(counter.bytes, ctrKey0B, cryptedCounter.bytes);
		}
		buffer[i + offset] ^= cryptedCounter.bytes[counterLoc]; //decrypt one byte
		decryptedPos++;
		counterLoc++;
	}
}