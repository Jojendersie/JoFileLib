#include "plattform.hpp"
#include "fileutils.hpp"
#include <algorithm>

//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>

#ifdef JO_WINDOWS
#undef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#include <windows.h>
//#include <direct.h>
//#include <io.h>


namespace Jo {
namespace Files {
namespace Utils {

	// ********************************************************************* //
	// Refreshes the lists of names.
	void FileEnumerator::Reset( const std::string& _directory )
	{
		// Clear old
		m_directories.clear();
		m_files.clear();

		m_currentDirectory = _directory;
		HANDLE handle;
		WIN32_FIND_DATA data;
 
		char charBuf[512];
		sprintf(charBuf, "%s\\*", _directory.c_str());
		handle = FindFirstFile(charBuf, &data);
		do {
			// Skip ".", ".." and empty names
			if( (data.cFileName[0]>0) && !( (data.cFileName[0]=='.') && ( (data.cFileName[1]=='.' && data.cFileName[2]==0) || data.cFileName[1]==0 ) ))
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


	// ********************************************************************* //
	// Takes an arbitrary file name makes sure that is has a full path.
	/*std::string MakeAbsolute( const std::string& _name )
	{
		int startDir = _open(".", 0x0000);
		std::string path;
		if(startDir != -1)
		{
			if (!chdir(_name.c_str()))	// Change to directory
			{
				getcwd(path);			// And get its path
				fchdir(startDir);		// And change back
			}
			_close(startDir);
		}
		return path;
	}

	// ********************************************************************* //
	// Extracts the part after the last '.'.
	std::string GetExtension( const std::string& _name )
	{
	}

	// ********************************************************************* //
	// Returns the absolute path of the file but without the files name itself.
	std::string GetDirectory( const std::string& _name )
	{
	}

	// ********************************************************************* //
	// Returns the file name without the directory
	std::string GetFileName( const std::string& _name )
	{
	}*/

};
};
};

#endif