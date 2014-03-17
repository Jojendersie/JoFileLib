#include "plattform.hpp"
#include "fileutils.hpp"
#include <algorithm>

#ifdef JO_UNIX

#include <dirent.h>
#include <sys/stat.h>

namespace Jo {
namespace Files {
namespace Utils {

	// ********************************************************************* //
	// Create a directory. The name should not contain a file name.
	void MakeDir( const std::string& _name )
	{
		// Read/write/search permissions for owner and group, and with
		// read/search permissions for others.
		mkdir(_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	}


	// ********************************************************************* //
	// Refreshes the lists of names.
	void FileEnumerator::Reset( const std::string& _directory )
	{
		// Clear old
		m_directories.clear();
		m_files.clear();

		DIR* directory = opendir(_directory.c_str());
		m_currentDirectory = _directory;

		if( !directory ) throw std::string("Cannot open '" + _directory + "' as directory for enumeration.");

		struct dirent* entry = readdir(directory);
		while( entry != nullptr )
		{
			// Skip ".", ".." and empty names
			if( (entry->d_name[0] != 0) && !( (entry->d_name[0]=='.') && ( (entry->d_name[1]=='.' && entry->d_name[2]==0) || entry->d_name[1]==0 ) ))
			{
				struct stat state;
				std::string path = _directory + '/' + entry->d_name;
				stat(path.c_str(), &state);
				if( S_ISDIR(state.st_mode) )
				{
					m_directories.push_back( entry->d_name );
				} else {
					// For real files remember name and size
					FileDesc tmp;
					tmp.name = entry->d_name;
					tmp.size = state.st_size;
					m_files.push_back( tmp );
				}
			}

			entry = readdir(directory);
		}

		closedir( directory );

		// Sort	alphabetically
		std::sort( m_directories.begin(), m_directories.end() );
		std::sort( m_files.begin(), m_files.end(), [](const FileDesc& a, const FileDesc& b){ return a.name<b.name; } );
	}

} // namespace Utils
} // namespace Files
} // namespace Jo

#endif