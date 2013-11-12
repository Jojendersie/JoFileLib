#include "plattform.hpp"
#include "fileutils.hpp"
#include <algorithm>

#ifdef JO_WINDOWS
#undef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#include <windows.h>

namespace Jo {
namespace Files {
namespace Utils {

	// ********************************************************************* //
	// Reads all filenames form the given directory.
	FileEnumerator::FileEnumerator( const std::string& _directory )
	{
		Reset( _directory );
	}

	// ********************************************************************* //
	// Refreshes the lists of names.
	void FileEnumerator::Reset( const std::string& _directory )
	{
		HANDLE handle;
		WIN32_FIND_DATA data;
 
		char charBuf[512];
		sprintf(charBuf, "%s\\*", _directory.c_str());
		handle = FindFirstFile(charBuf, &data);
		do {
			// Skip ".", ".." and empty names
			if (!( (data.cFileName[0]=='.')
					&& ( (data.cFileName[1]=='.' && data.cFileName[2]==0)
					   || data.cFileName[1]==0 ) ))
			if( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				m_directories.push_back( data.cFileName );
			} else {
				// For real files remember name and size
				FileDesc tmp;
				tmp.name = data.cFileName;
				tmp.size = uint64_t(data.nFileSizeHigh) << 32 | data.nFileSizeLow;
				m_files.push_back( tmp );
			}
		} while( FindNextFile(handle, &data) );

		FindClose(handle);

		// Sort	alphabetically
		std::sort( m_directories.begin(), m_directories.end() );
		std::sort( m_files.begin(), m_files.end(), [](const FileDesc& a, const FileDesc& b){ return a.name<b.name; } );
	}
};
};
};

#endif