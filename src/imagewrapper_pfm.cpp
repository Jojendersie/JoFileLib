#include <cstdint>
#include "imagewrapper.hpp"
#include "jofilelib.hpp"
#include "plattform.hpp"

namespace Jo {
namespace Files {

	void ImageWrapper::ReadPFM( const IFile& _file )
	{
		char format[2];
		_file.Read( 2, format );

		// Read and evaluate format line.
		if( format[0] != 'P' ) throw std::string("[ImageWrapper::ReadPFM] First line is not 'Pf' or 'PF'.");
		if( format[1] == 'F' ) m_numChannels = 3;
		else if( format[1] == 'f' ) m_numChannels = 1;
		else throw std::string("[ImageWrapper::ReadPFM] Expected 'f' or 'F' as specification.");

		// Read dimensions
		m_width = StreamReader::ReadASCIIInt( _file );
		m_height = StreamReader::ReadASCIIInt( _file );

		// Read scaling / endianness
		float scale = StreamReader::ReadASCIIFloat( _file );

		// Allocate a buffer to read into
		size_t num = m_width * m_height * m_numChannels;
		size_t size = num * sizeof(float);
		m_buffer = (uint8_t*)malloc(size);

		// Read block data
		_file.Read( size, m_buffer );

		// Correct endianness
		if( scale > 0.0f && IsLittleEndian() )
		{
			for( size_t i = 0; i < num; ++i )
				((float*)m_buffer)[i] = ConvertEndian(((float*)m_buffer)[i]);
		}

		// Multiply all values
		if( abs(scale) != 1.0 )
		{
			for( size_t i = 0; i < num; ++i )
				((float*)m_buffer)[i] *= scale;
		}
	}

	void ImageWrapper::WritePFM( IFile& _file ) const
	{
		// Check if the current image can be saved
		if( m_numChannels != 1 && m_numChannels != 3 )
			throw std::string("[ImageWrapper::WritePFM] The PFM format only supports greyscale and RGB");

		// Write first header line: PF of Pf
		if( m_numChannels == 1 )
			_file.Write( "Pf\n", 3 );
		else _file.Write( "PF\n", 3 );

		// Write dimensions line
		_file.Write( std::to_string(m_width) );
		_file.WriteU8( ' ' );
		_file.Write( std::to_string(m_height) );
		_file.WriteU8( '\n' );

		// Always use scale factor 1, encode the endianness
		if( IsLittleEndian() ) _file.Write( "-1.0\n", 5 );

		// Write the data just as it is (endianness is not our concern)
		_file.Write(m_buffer, m_width * m_height * m_numChannels * sizeof(float));
	}

} // namespace Files
} // namespace Jo