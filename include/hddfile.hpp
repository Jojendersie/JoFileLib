#pragma once

#include "file.hpp"
#include <string>
#include <cstdio>

namespace Jo {
namespace Files {

	/**************************************************************************//**
	 * \class	Files::HDD
	 * \brief	Read and write in a real file on hard disk.
	 * \details	The file is buffered with a 4096 byte buffer.
	 *****************************************************************************/
	class HDDFile: public IFile
	{
	protected:
		/// \brief C standard library file which is wrapped.
		FILE* m_file;
		int m_pendingWriteBytes;
	public:
		/// \brief Open a file on hard disk.
		/// \param [in] _name Name and path to a file on disk.
		/// \param [in] _readOnly Try to open file in read mode. This will fail
		///		with an exception if the file does not exists. To open a file
		///		in write mode may also fail if permission is denied.
		/// \param [in] _bufferSize Write operations are done buffered where
		///		the default buffer size is 4KB.
		HDDFile( const std::string& _name, bool _readOnly, int _bufferSize = 4096 );

		~HDDFile();

		virtual void Read( uint64_t _iNumBytes, void* _To ) const override;
		virtual void Write( const void* _From, uint64_t _iNumBytes ) override;

		/// \details Seek can only jump within the existing file.
		virtual void Seek( uint64_t _iNumBytes, SeekMode _Mode = SeekMode::SET ) const override;
	};
};
};