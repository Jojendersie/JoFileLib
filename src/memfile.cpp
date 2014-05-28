#include "memfile.hpp"
#include <cstring>	// memcpy
#include <string>
#include <algorithm>

namespace Jo {
namespace Files {

	// Uses an existing memory for file read access.
	MemFile::MemFile( const void* _memory, uint64_t _size ) :
		IFile( _size, true, false )
	{
		m_buffer = (void*)_memory;
		m_ownsMemory = false;
		m_capacity = _size;
	}

	// Creates a file of size 0 with read and write access.
	MemFile::MemFile( uint64_t _capacity ) :
		IFile( 0, true, true )
	{
		m_buffer = malloc(size_t(_capacity));
		m_ownsMemory = true;
		m_capacity = _capacity;
	}

	MemFile::MemFile( MemFile&& _file ) :
		IFile( _file ),
		m_buffer( _file.m_buffer ),
		m_capacity( _file.m_capacity ),
		m_ownsMemory( _file.m_ownsMemory )
	{
		_file.m_buffer = nullptr;
		_file.m_ownsMemory = false;
	}

	MemFile::~MemFile()
	{
		if( m_ownsMemory ) free( m_buffer );
		m_buffer = nullptr;
		m_capacity = 0;
	}

	const MemFile& MemFile::operator = ( MemFile&& _file )
	{
		// Avoid memory leak
		this->~MemFile();

		new (this) MemFile(std::move(_file));
		return *this;
	}

	void MemFile::Read( uint64_t _numBytes, void* _to ) const
	{
		// Test if read possible
		if( m_cursor + _numBytes > m_size ) {
			char charBuf[128];
			sprintf( charBuf, "Cannot read %llu bytes. Only %llu left in file.", _numBytes, m_size-m_cursor );
			throw std::string(charBuf);
		}

		memcpy( _to, (uint8_t*)m_buffer+m_cursor, size_t(_numBytes) );
		m_cursor += _numBytes;
	}

	uint8_t MemFile::Next() const
	{
		++m_cursor;
		return ((uint8_t*)m_buffer)[m_cursor-1];
	}

	void MemFile::Write( const void* _from, uint64_t _numBytes )
	{
		memcpy( Reserve(_numBytes), _from, size_t(_numBytes) );
	}

	void* MemFile::Reserve( uint64_t _numBytes )
	{
		if( !m_writeAccess ) throw std::string("No write access.");

		// Resize memory
		if( m_cursor + _numBytes > m_capacity )
		{
			// Increase to 2x or expect more writes of the current size.
			m_capacity = std::max( m_cursor + _numBytes * 2, m_capacity*2 );
			m_buffer = realloc( m_buffer, size_t(m_capacity) );
		}

		void* address = (uint8_t*)m_buffer + m_cursor;

		m_cursor += _numBytes;
		// The write could be some where in the middle through seek.
		m_size = std::max( m_size, m_cursor );

		return address;
	}

	void MemFile::Seek( uint64_t _numBytes, SeekMode _mode ) const
	{
		// Update only the m_cursor
		switch( _mode )
		{
		case SeekMode::MOVE_BACKWARD:
			// Do not underflow the 0
			m_cursor = m_cursor > _numBytes ? m_cursor - _numBytes : 0;
			break;
		case SeekMode::MOVE_FORWARD:
			m_cursor += _numBytes;
			break;
		case SeekMode::SET:
			// Don't ask. Even seeking into non existing lokations allowed
			// (random write access)
			m_cursor = _numBytes;
			break;
		}
	}

} // namespace Files
} // namespace Jo