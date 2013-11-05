#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Jo {
namespace Files {
namespace Utils {

	/// The files are ordered alphabetically in ascending order.
	class FileEnumerator
	{
		struct FileDesc {
			std::string name;
			uint64_t size;
		};
		std::vector<FileDesc>		m_Files;
		std::vector<std::string>	m_Directories;
	public:
		/// \brief Reads all filenames form the given directory.
		/// \details All the names are buffered so deletions or creations of
		///		files during iteration are not recognized.
		FileEnumerator( const std::string& _Directory );

		/// \brief Refreshes the lists of names.
		/// \param [in] _Directory Name of any directory to reload the list from.
		void Reset( const std::string& _Directory );

		int GetNumFiles() const					{ return m_Files.size(); }
		int GetNumDirectories() const			{ return m_Directories.size(); }

		uint64_t GetFileSize( int _iIndex ) const					{ return m_Files[_iIndex].size; }

		/// \brief Returns the file name without any path.
		///
		const std::string& GetFileName( int _iIndex ) const			{ return m_Files[_iIndex].name; }

		const std::string& GetDirectoryName( int _iIndex ) const	{ return m_Directories[_iIndex]; }

		/// \brief Returns the name of the directory whichs content is enumerated.
		///
		const std::string& GetCurrentDirectoryName() const;
	};

	/// \brief Takes an arbitrary file name makes sure that is has a full path.
	/// \param [in] _Name Arbitrary file name.
	std::string MakeAbsolute( const std::string& _Name );

	/// \brief Extracts the part after the last '.'.
	/// \param [in] _Name Arbitrary file name.
	std::string GetExtension( const std::string& _Name );

	/// \brief Returns the absolute path of the file but without the
	///		files name itself.
	/// \param [in] _Name Arbitrary file name.
	std::string GetDirectory( const std::string& _Name );

	/// \brief Returns the file name without the directory
	/// \param [in] _Name Arbitrary file name.
	std::string GetFileName( const std::string& _Name );
};
};
};