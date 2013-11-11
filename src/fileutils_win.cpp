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
	FileEnumerator::FileEnumerator( const std::string& _Directory )
	{
		Reset( _Directory );
	}

	// ********************************************************************* //
	// Refreshes the lists of names.
	void FileEnumerator::Reset( const std::string& _Directory )
	{
		HANDLE Handle;
		WIN32_FIND_DATA Data;
 
		char acBuf[512];
		sprintf(acBuf, "%s\\*", _Directory.c_str());
		Handle = FindFirstFile(acBuf, &Data);
		do {
			// Skip ".", ".." and empty names
			if (!( (Data.cFileName[0]=='.')
					&& ( (Data.cFileName[1]=='.' && Data.cFileName[2]==0)
					   || Data.cFileName[1]==0 ) ))
			if( Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				m_Directories.push_back( Data.cFileName );
			} else {
				// For real files remember name and size
				FileDesc tmp;
				tmp.name = Data.cFileName;
				tmp.size = uint64_t(Data.nFileSizeHigh) << 32 | Data.nFileSizeLow;
				m_Files.push_back( tmp );
			}
		} while( FindNextFile(Handle, &Data) );

		FindClose(Handle);

		// Sort	alphabetically
		std::sort( m_Directories.begin(), m_Directories.end() );
		std::sort( m_Files.begin(), m_Files.end(), [](const FileDesc& a, const FileDesc& b){ return a.name<b.name; } );
	}
};
};
};

#endif