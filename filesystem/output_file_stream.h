// Copyright 2002-2004 Frozenbyte Ltd.

#ifndef INCLUDED_FILESYSTEM_OUTPUT_FILE_STREAM_H
#define INCLUDED_FILESYSTEM_OUTPUT_FILE_STREAM_H

#ifndef INCLUDED_FILESYSTEM_OUTPUT_STREAM_H
#include "output_stream.h"
#endif

namespace frozenbyte {
namespace filesystem {

struct OutputFileStreamBufferData;

class OutputFileStreamBuffer: public IOutputStreamBuffer
{
	OutputFileStreamBufferData* data;

public:
	OutputFileStreamBuffer(const std::string &fileName);
	~OutputFileStreamBuffer();

	void putByte(unsigned char c);
};

OutputStream createOutputFileStream(const std::string &fileName);

} // end of namespace filesystem
} // end of namespace frozenbyte

#endif
