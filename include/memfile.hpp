#pragma once

#include "file.hpp"

namespace Jo {
namespace Files {

	/**************************************************************************//**
	 * \class	Files::MemFile
	 * \brief	Set a default memory to be interpreted as a file.
	 * \details	
	 *****************************************************************************/
	class MemFile: public IFile
	{
	protected:
		/// \brief The whole file in memory.
		/// \details The buffer can have a different capacity than it is filled
		///		with real data.
		void* m_pBuffer;
		uint64_t m_iCapacity;
	public:
		/// \brief Uses an existing memory for file read access.
		/// \param [in] _pMemory The memory which should be wrapped. Do not
		///		delete before this MemFile. The file will access directly to
		///		the original memory.
		/// \param [in] _iSize Size of the part which should be wrapped. This
		///		must not necessaryly be the whole memory.
		MemFile( const void* _pMemory, uint64_t _iSize );

		/// \brief Creates a file of size 0 with read and write access.
		/// \param [in] _iCapacity Initial capacity. The buffer is resized if
		///		necessary. The default value is 4 KB. If you can (over)
		///		estimate the target size this will be performanter.
		MemFile( uint64_t _iCapacity = 4096 );

		virtual void Read( uint64_t _iNumBytes, void* _To ) const override;
		virtual void Write( const void* _From, uint64_t _iNumBytes ) override;

		/// \details Seek can even jump to locations > size for random write
		///		access. Reading at such a location will fail.
		virtual void Seek( uint64_t _iNumBytes, SeekMode _Mode = SeekMode::SET ) const override;

		void* GetBuffer()				{ return m_pBuffer; }
		const void* GetBuffer() const	{ return m_pBuffer; }
	};
};
};