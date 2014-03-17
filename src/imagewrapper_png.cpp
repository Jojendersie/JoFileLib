#include <cstdint>
#include "imagewrapper.hpp"
#include "../dependencies/libpng166/png.h"
#include "../dependencies/zlib128/zlib.h"
#include "jofilelib.hpp"

namespace Jo {
namespace Files {

	static void ReadCallback(png_structp _png, png_bytep _data, png_size_t _length)
	{
		IFile* file = (IFile*)png_get_io_ptr(_png);
		file->Read( _length, _data );
	}

	static void WriteCallback(png_structp _png, png_bytep _data, png_size_t _length)
	{
		IFile* file = (IFile*)png_get_io_ptr(_png);
		file->Write( _data, _length );
	}

	static void FlushCallback(png_structp _png)
	{
		// Ignore flush
	}

	static void ErrorCallback( png_structp _png, png_const_charp _errorMessage )
	{
		throw std::string(_errorMessage);
	}

	// A small wrapper for the two png pointers. This is used to achieve a clean
	// memory on exception
	struct PNGR {
		png_structp handle;
		png_infop info;
		PNGR() : handle(nullptr), info(nullptr)	{}
		~PNGR()	{png_destroy_read_struct(&handle, &info, nullptr);}
	};

	void ImageWrapper::ReadPNG( const IFile& _file )
	{
		PNGR png;
		png.handle = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, ErrorCallback, nullptr );
		if(!png.handle) throw std::string("[ImageWrapper::ReadPNG] Allocation of png_struct failed.");
		png.info = png_create_info_struct(png.handle);
		if(!png.info) throw std::string("[ImageWrapper::ReadPNG] Allocation of png_info failed.");

		// Set new callbacks for I/O
		png_set_read_fn( png.handle, (png_voidp)&_file, ReadCallback );

		// Read first 8 bytes and check if it is realy png
		uint8_t test[8];
		_file.Read( 8, test );
		if(png_sig_cmp(test, 0, 8)) throw std::string("[ImageWrapper::ReadPNG] File does not contain a valid png file.");
		png_set_sig_bytes(png.handle, 8);

		// Read header
		png_read_info(png.handle, png.info);
		int colorType, interlacedMethod, compressionMethod, filterMethod;
		png_get_IHDR(png.handle, png.info, &m_width, &m_height, &m_bitDepth, &colorType,
			&interlacedMethod, &compressionMethod, &filterMethod);
		// Allocate memory for full resolution image
		m_numChannels = png_get_channels(png.handle, png.info);
		int pixelSize = m_numChannels * m_bitDepth / 8;
		m_buffer = (uint8_t*)malloc( pixelSize * m_width * m_height );

		// Handle interlacing automatically
		int numPasses = png_set_interlace_handling(png.handle);
		
		// Read in all sub images if interlaced
		for( int i=0; i<numPasses; ++i )
		{
			// Compute offset to largest image if interlaced format
			int startX=0, startY=0;
			int stepX=1, stepY=1;
		
			// Read everything into one memory block. Unfortunatelly there
			// are only calls which expect arrays of rows. So read linewise.
			for( unsigned y=startY; y<m_height; y+=stepY )
			{
				uint8_t* rowStart = m_buffer + m_width * pixelSize * y;
				png_read_row(png.handle, rowStart, nullptr);
			}
		}

		// png format has always unsigned integers
		m_channelType = ChannelType::UINT;

		// Resources are release by the PNGR destructor (even on throw)
	}


	// A small wrapper for the two png pointers. This is used to achieve a clean
	// memory on exception
	struct PNGW {
		png_structp handle;
		png_infop info;
		PNGW() : handle(nullptr), info(nullptr)	{}
		~PNGW()	{png_destroy_write_struct(&handle, &info);}
	};

	void ImageWrapper::WritePNG( IFile& _file ) const
	{
		PNGW png;

		// Test if the current format can be represented by png
		if( m_channelType == ChannelType::FLOAT ) throw std::string("[ImageWrapper::WritePNG] PNG does not support floating point channels.");
		if( m_numChannels > 1 && m_bitDepth < 8 ) throw std::string("[ImageWrapper::WritePNG] PNG does not support 1,2 or 4 bit color textures.");
		if( m_bitDepth == 32 )  throw std::string("[ImageWrapper::WritePNG] PNG does not support 32bit channels.");
		// Silent cast int and uint

		png.handle = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, ErrorCallback, nullptr );
		if(!png.handle) throw std::string("[ImageWrapper::WritePNG] Allocation of png_struct failed.");
		png.info = png_create_info_struct(png.handle);
		if(!png.info) throw std::string("[ImageWrapper::WritePNG] Allocation of png_info failed.");

		// Set new callbacks for I/O
		png_set_write_fn( png.handle, (png_voidp)&_file, WriteCallback, FlushCallback );

		// Use best filter available
		png_set_filter(png.handle, 0, PNG_FILTER_VALUE_PAETH);

		// Write header
		uint32_t colorType;
		switch (m_numChannels)
		{
			case 1: colorType = PNG_COLOR_TYPE_GRAY; break;
			case 2: colorType = PNG_COLOR_TYPE_GRAY_ALPHA; break;
			case 3: colorType = PNG_COLOR_TYPE_RGB; break;
			case 4: colorType = PNG_COLOR_TYPE_RGB_ALPHA; break;
		}
		png_set_IHDR(png.handle, png.info, m_width, m_height, m_bitDepth, colorType, PNG_INTERLACE_NONE,
					PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_write_info(png.handle, png.info);

		// Write data row by row
		int pixelSize = m_numChannels * m_bitDepth / 8;
		for( unsigned y=0; y<m_height; ++y )
		{
			uint8_t* rowStart = m_buffer + m_width * pixelSize * y;
			png_write_row(png.handle, rowStart);
		}

		png_write_end(png.handle, nullptr);

		// Resources are release by the PNGW destructor (even on throw)
	}

} // namespace Files
} // namespace Jo