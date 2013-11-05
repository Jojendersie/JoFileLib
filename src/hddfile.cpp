#include "hddfile.hpp"
#include <algorithm>

namespace Jo {
namespace Files {

	HDDFile::HDDFile( const std::string& _name, bool _readOnly, int _bufferSize ) :
		IFile(0, _readOnly, !_readOnly), m_pendingWriteBytes(0)
	{
		if( _readOnly ) {
			m_file = fopen( _name.c_str(), "rb" );
		} else {
			m_file = fopen( _name.c_str(), "wb" );
		}

		if(!m_file) throw "Failed to open file '" + _name + "'";

		// Allocate temporary write buffer.
		if( !_readOnly )
			std::setvbuf( m_file, nullptr, _IOFBF, 4096 );


		// Determine file size
		fseek( m_file, 0, SEEK_END );
		m_iSize = ftell( m_file );
		fseek( m_file, 0, SEEK_SET );
	}

	HDDFile::~HDDFile()
	{
		// Release resources
		fflush( m_file );
		fclose( m_file );
	}

	void HDDFile::Read( uint64_t _iNumBytes, void* _To ) const
	{
		if( !m_bReadAccess ) throw std::string("No read access.");

		// Just read there cannot be any pending write
		fread( _To, size_t(_iNumBytes), 1, m_file );
		m_iCursor += _iNumBytes;
	}

	void HDDFile::Write( const void* _From, uint64_t _iNumBytes )
	{
		if( !m_bWriteAccess ) throw std::string("No write access.");

		fwrite( _From, size_t(_iNumBytes), 1, m_file );
		m_iCursor += _iNumBytes;
	}

	void HDDFile::Seek( uint64_t _iNumBytes, SeekMode _Mode ) const
	{
		// Update the cursor and than set
		switch(_Mode)
		{
		case SeekMode::SET: m_iCursor = _iNumBytes; break;
		case SeekMode::MOVE_FORWARD: m_iCursor += _iNumBytes; break;
		case SeekMode::MOVE_BACKWARD: m_iCursor -= _iNumBytes; break;
		}

		// Set 64 index on each system
		fseek( m_file, 0, SEEK_SET );
		int64_t offset = m_iCursor;
		while( offset > 0 )
		{
			long stepSize = (long)std::min( offset, (int64_t)std::numeric_limits<long>::max());
			fseek( m_file, stepSize, SEEK_CUR );
			offset -= stepSize;
		}
	}
};
};