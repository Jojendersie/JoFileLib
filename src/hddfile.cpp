#include "hddfile.hpp"
#include "fileutils.hpp"
#include <algorithm>

namespace Jo {
namespace Files {

	HDDFile::HDDFile( const std::string& _name, ModeFlags _flags, int _bufferSize ) :
		IFile(0, true, true), m_pendingWriteBytes(0)
	{
		const char* modeStr = "r+b";

		m_file = fopen( _name.c_str(), modeStr );

		// In write mode it could be that the directory is missing
		if(!m_file && (_flags & CREATE_FILE))
		{
			// Search for the directory
			std::string dir = Utils::GetDirectory(_name);
			if( !Utils::Exists(_name) )
			{
				// Create missing directory
				Utils::MakeDir(dir);
				// Retry: w+ might destroy content, but we are sure nothing
				// like that exists!
				m_file = fopen( _name.c_str(), "w+b" );
			}
		}

		// Retry with read only (permissions?)
		if(!m_file)
		{
			m_writeAccess = false;
			modeStr = "rb";
			m_file = fopen( _name.c_str(), modeStr );
		}

		if(!m_file) throw "Failed to open file '" + _name + "'";

		// Allocate temporary write buffer.
		std::setvbuf( m_file, nullptr, _IOFBF, _bufferSize );


		// Determine file size
		fseek( m_file, 0, SEEK_END );
		m_size = ftell( m_file );
		if( !(_flags & APPEND) )
			fseek( m_file, 0, SEEK_SET );
	}

	HDDFile::~HDDFile()
	{
		// Release resources
		fflush( m_file );
		fclose( m_file );
	}

	void HDDFile::Read( uint64_t _numBytes, void* _to ) const
	{
		if( !m_readAccess ) throw std::string("No read access.");

		// Just read there cannot be any pending write
		fread( _to, size_t(_numBytes), 1, m_file );
		m_cursor += _numBytes;
	}

	void HDDFile::Write( const void* _from, uint64_t _numBytes )
	{
		if( !m_writeAccess ) throw std::string("No write access.");

		fwrite( _from, size_t(_numBytes), 1, m_file );
		m_cursor += _numBytes;
	}

	void HDDFile::Seek( uint64_t _numBytes, SeekMode _mode ) const
	{
		// Update the cursor and than set
		switch(_mode)
		{
		case SeekMode::SET: m_cursor = _numBytes; break;
		case SeekMode::MOVE_FORWARD: m_cursor += _numBytes; break;
		case SeekMode::MOVE_BACKWARD: m_cursor -= _numBytes; break;
		}

		// Set 64 index on each system
		fseek( m_file, 0, SEEK_SET );
		int64_t offset = m_cursor;
		while( offset > 0 )
		{
			long stepSize = (long)std::min( offset, (int64_t)std::numeric_limits<long>::max());
			fseek( m_file, stepSize, SEEK_CUR );
			offset -= stepSize;
		}
	}

	void HDDFile::Flush()
	{
		fflush( m_file );
	}

};
};