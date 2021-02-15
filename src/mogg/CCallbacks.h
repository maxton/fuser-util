#pragma once

#ifndef _OV_FILE_H_
#include "XiphTypes.h"
#endif
#include <fstream>
#include <istream>

// Callbacks using standard C FILE* as a datasource.
extern ov_callbacks cCallbacks;

template<typename T> int close_stream(void* datasource);
template<> int close_stream<std::ifstream>(void* datasource);
template<> int close_stream<std::istream>(void* datasource);

// Callbacks using a C++ i(f)stream* as a datasource.
template<typename T>
ov_callbacks cppCallbacks = {
	[](void *ptr, size_t size, size_t nmemb, void *datasource) -> size_t {
		auto *file = static_cast<T*>(datasource);
		file->read((char*)ptr, size * nmemb);
		return file->gcount() / size;
	},
	[](void *datasource, ogg_int64_t offset, int whence) -> int {
		auto *file = static_cast<T*>(datasource);
		if (file->eof()) file->clear();
		
		std::ios_base::seekdir way;
		switch (whence) {
			case SEEK_SET: way = file->beg; break;
			case SEEK_CUR: way = file->cur; break;
			case SEEK_END: way = file->end; break;
		}
		file->seekg(offset, way);
		return file->fail();
	},
	close_stream<T>,
	[](void *datasource) -> long {
		auto *file = static_cast<T*>(datasource);
		return file->tellg();
	}
};