#pragma once

#include <cstdint>

namespace Jo {
namespace Files {

	/**************************************************************************//**
	 * \class	Jo::Files::IFile
	 * \brief	A file system abstraction layer.
	 * \details	All files derived from this one should support random access read
	 *			and write operations.
	 *****************************************************************************/
	class IFile
	{
	protected:
		uint64_t m_iSize;
		mutable uint64_t m_iCursor;
		bool m_bWriteAccess;
		bool m_bReadAccess;

		IFile( uint64_t _iSize, bool _bRead, bool _bWrite ) :
			m_iSize( _iSize ), m_iCursor( 0 ), m_bReadAccess( _bRead ), m_bWriteAccess( _bWrite )
		{}
	public:
		enum struct SeekMode {
			SET,
			MOVE_FORWARD,
			MOVE_BACKWARD
		};

		virtual bool Read( uint64_t _iNumBytes, void* _To ) const = 0;
		virtual void Write( const void* _From, uint64_t _iNumBytes ) = 0;
		virtual void Seek( uint64_t _iNumBytes, SeekMode _Mode = SeekMode::SET ) const = 0;

		/// \brief Returns the cursor position within the file.
		/// \return A cursor position with large file support.
		uint64_t GetCursor() const		{ return m_iCursor; }

		/// \brief Get the actual file size.
		/// \details A write operation must only update the size after succeeding.
		/// \return Returns the actual file size.
		uint64_t GetSize() const		{ return m_iSize; }

		/// \brief Was this file opened with write access?
		/// \return true if things can be written into this file.
		bool CanWrite()		{ return m_bWriteAccess; }

		bool IsEof()		{ return m_iSize == m_iCursor; }
	};
};
};