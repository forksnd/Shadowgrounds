// Copyright 2002-2004 Frozenbyte Ltd.

#ifndef INCLUDED_FILESYSTEM_MEMORY_STREAM_H
#define INCLUDED_FILESYSTEM_MEMORY_STREAM_H

#include "input_stream.h"
#include "output_stream.h"

namespace frozenbyte {
namespace filesystem {

struct MemoryStreamBufferData;

class MemoryStreamBuffer: public IInputStreamBuffer, public IOutputStreamBuffer
{
	MemoryStreamBufferData* data;

public:
	MemoryStreamBuffer();
	~MemoryStreamBuffer();

	unsigned char popByte();
	bool isEof() const;
	int getSize() const;

	void putByte(unsigned char byte);
	void popBytes(char *buffer, int bytes);
};

} // end of namespace filesystem
} // end of namespace frozenbyte

#endif
