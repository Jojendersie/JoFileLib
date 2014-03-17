#include "plattform.hpp"
#include "fileutils.hpp"
#include <algorithm>
#include <sys/stat.h>

namespace Jo {
namespace Files {
namespace Utils {

	/// Changes all \ into / to have higher compatibility
	// TODO: windows only?
	static void MakeCorrect( std::string& _name )
	{
		replace(_name.begin(), _name.end(), '\\', '/');
	}

	// ********************************************************************* //
	// Splits a given path name into directory and file.
	void SplitName( const std::string& _name, std::string& _dir, std::string& _file )
	{
		std::string name = _name;
		MakeCorrect(name);
		// Find the last slash
		size_t slashPosition = name.rfind('/');

		if(slashPosition != std::string::npos)
		{
			_dir = name.substr(0, slashPosition+1);
			_file = name.substr(slashPosition+1);
		} else {
			_file = name;
		}
	}

	// ********************************************************************* //
	// Returns the path of the file but without the file's name itself.
	std::string GetDirectory( const std::string& _name )
	{
		std::string name = _name;
		MakeCorrect(name);
		// Find the last slash
		size_t slashPosition = name.rfind('/');
		if(slashPosition != std::string::npos)
			return name.substr(0, slashPosition+1);
		else return std::string("");
	}

	// ********************************************************************* //
	// Tests if a file or directory exists.
	bool Exists( const std::string& _name )
	{
		struct stat buf;
		return 0 == stat( _name.c_str(), &buf );
	}

	// ********************************************************************* //
	// Compare paths/files if they are equal.
	bool IsEqual( std::string _name0, std::string _name1 )
	{
		if( _name0.empty() || _name1.empty() ) return false;

		// Remove / or \ at the end
		bool check = true;
		while( check )
		{
			if( _name0.back() == '/' ) _name0.pop_back();
			else if( _name0.back() == '\\' ) _name0.pop_back();
			else check = false;		// Found some non-slash
			if( _name0.empty() ) return false;
		}
		check = true;
		while( check )
		{
			if( _name1.back() == '/' ) _name1.pop_back();
			else if( _name1.back() == '\\' ) _name1.pop_back();
			else check = false;		// Found some non-slash
			if( _name1.empty() ) return false;
		}

		// Compare the stat structs of both files
		struct stat buf0;
		struct stat buf1;
		if( stat( _name0.c_str(), &buf0 ) != 0 ) return false;
		if( stat( _name1.c_str(), &buf1 ) != 0 ) return false;

		return buf0.st_dev == buf1.st_dev && buf0.st_ino == buf1.st_ino;
	}





	// ********************************************************************* //
	// Reads all filenames form the given directory.
	FileEnumerator::FileEnumerator( const std::string& _directory )
	{
		Reset( _directory );
	}

} // namespace Utils
} // namespace Files
} // namespace Jo