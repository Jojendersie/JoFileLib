#include "memfile.hpp"
#include <string>

namespace Jo {
namespace Files {

	// Uses an existing memory for file read access.
	MemFile::MemFile( const void* _pMemory, uint64_t _iSize ) :
		IFile( _iSize, true, false )
	{
		m_pBuffer = (void*)_pMemory;
		m_iCapacity = _iSize;
	}

	// Creates a file of size 0 with read and write access.
	MemFile::MemFile( uint64_t _iCapacity ) :
		IFile( 0, true, true )
	{
		m_pBuffer = malloc(_iCapacity);
		m_iCapacity = _iCapacity;
	}

	bool MemFile::Read( uint64_t _iNumBytes, void* _To ) const
	{
		// Test if read possible
		if( m_iCursor + _iNumBytes > m_iSize ) {
			char acBuf[128];
			sprintf( acBuf, "Cannot read %llu bytes. Only %llu left in file.", _iNumBytes, m_iSize-m_iCursor );
			throw std::string(acBuf);
		}

		memcpy( _To, (uint8_t*)m_pBuffer+m_iCursor, _iNumBytes );
		m_iCursor += _iNumBytes;
	}

	void MemFile::Write( const void* _From, uint64_t _iNumBytes )
	{
		if( !m_bWriteAccess ) throw std::string("No write access.");

		// Resize memory
		if( m_iCursor + _iNumBytes > m_iCapacity )
		{
			// Increase to 2x or await more writes of the current size.
			m_iCapacity = std::max( m_iCursor + _iNumBytes * 2, m_iCapacity*2 );
			m_pBuffer = realloc( m_pBuffer, m_iCapacity );
		}

		memcpy( (uint8_t*)m_pBuffer + m_iCursor, _From, _iNumBytes );
		m_iCursor += _iNumBytes;
		// The write could be some where in the middle through seek.
		m_iSize = std::max( m_iSize, m_iCursor );
	}

	void MemFile::Seek( uint64_t _iNumBytes, SeekMode _Mode ) const
	{
		// Update only the m_iCursor
		switch( _Mode )
		{
		case SeekMode::MOVE_BACKWARD:
			// Do not underflow the 0
			m_iCursor = m_iCursor > _iNumBytes ? m_iCursor - _iNumBytes : 0;
			break;
		case SeekMode::MOVE_FORWARD:
			m_iCursor += _iNumBytes;
			break;
		case SeekMode::SET:
			// Don't ask. Even seeking into non existing lokations allowed
			// (random write access)
			m_iCursor = _iNumBytes;
			break;
		}
	}

};
};