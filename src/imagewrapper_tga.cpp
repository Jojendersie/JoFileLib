#include <cstdint>
#include <scopedpointer.hpp>
#include "imagewrapper.hpp"
#include "jofilelib.hpp"
#include "platform.hpp"

namespace Jo {
namespace Files {
	
#pragma pack( push, 1 )
	struct Header {
		uint8_t idLength;			///< Length of vendor specific id field
		uint8_t colorMapType;		///< Whether a color map is included
		uint8_t imageType;			///< Compression and color types
		struct {
			uint16_t offset;		///< Offset from the file beginning?
			uint16_t length;		///< Number of entries
			uint8_t depth;			///< Number of bits per color map entry
		} colorMap;

		struct {
			uint16_t xOrigin;		///< Lower left corner. ???
			uint16_t yOrigin;		///< Lower left corner. ???
			uint16_t width;			///< Width in pixels
			uint16_t height;		///< height in pixels
			uint8_t pixelDepth;		///< Bits per pixel
			uint8_t descriptor;		///< bits 3-0 give the alpha channel depth, bits 5-4 give direction
		} image;
	};
#pragma pack( pop )


	// ********************************************************************* //
	bool ImageWrapper::IsTGA( const IFile& _file )
	{
		Header header;
		_file.Read( sizeof(Header), &header );
		_file.Seek( 0 );
		// There is no check-field in the tga header just test reasonable things
		return (header.imageType <= 3 || (header.imageType >= 9 && header.imageType <= 11))
			&& (header.colorMapType <= 1)
			&& ((header.colorMap.depth == 0) || (header.colorMap.depth == 15) || (header.colorMap.depth == 16) || (header.colorMap.depth == 24) || (header.colorMap.depth == 32))
			&& (header.colorMap.offset <= 273)
			&& ((header.image.pixelDepth == 1) || (header.image.pixelDepth == 8) || (header.image.pixelDepth == 15) || (header.image.pixelDepth == 16) || (header.image.pixelDepth == 24) || (header.image.pixelDepth == 32));
	}


	// ********************************************************************* //
	static void SetColor( uint8_t* _color, int _colorDepth, ImageWrapper& img, int _x, int _y )
	{
		if( _colorDepth <= 16 )
		{
			uint16_t color = AsLittleEndian(*(uint16_t*)_color);
			// Read BRG as 5 bits each
			img.Set( _x, _y, 2, (color & 0x001f) / 31.0f );
			img.Set( _x, _y, 1, (color & 0x03e0) / 31.0f );
			img.Set( _x, _y, 0, (color & 0x7c00) / 31.0f );
			// Read A as 1 bit
			if( _colorDepth == 16 )
				img.Set( _x, _y, 3, static_cast<float>(color & 0x8000) );
		}

		// Read BRG as 8 bits each
		// TODO: Check order of accesses
		img.Set( _x, _y, 2, _color[0] / 255.0f );
		img.Set( _x, _y, 1, _color[1] / 255.0f );
		img.Set( _x, _y, 0, _color[2] / 255.0f );
		// Read A as 8 bit
		if( _colorDepth == 32 )
			img.Set( _x, _y, 3, _color[3] / 255.0f );
	}

	void ImageWrapper::ReadTGA( const IFile& _file )
	{
		Header header;
		_file.Read( sizeof(Header), &header );
		if( header.imageType == 0 ) throw "[ReadTGA] Image without data!";
		// Find correct bit depth and channel number
		m_width = header.image.width;
		m_height = header.image.height;
		m_bitDepth = 8;
		m_channelType = ChannelType::UINT;
		ScopedPtr<uint8_t> colorMap;
		if( header.colorMapType == 1 )
		{
			// Must be an image type with a color map
			assert( header.imageType == 1 || header.imageType == 9 );

			// There should be a color map with BGR or BGRA
			if( header.colorMap.depth == 15 || header.colorMap.depth == 24 )
				m_numChannels = 3;
			else if( header.colorMap.depth == 16 || header.colorMap.depth == 32 )
				m_numChannels = 4;
			else throw "[ReadTGA] unknown color map format.";
			size_t colorMapSize = header.colorMap.length * ((header.colorMap.depth + 1)/8);
			colorMap = (uint8_t*)malloc(colorMapSize);

			_file.Seek( header.colorMap.offset );
			_file.Read( colorMapSize, colorMap );
		} else if( header.image.pixelDepth <= 8 )
			m_numChannels = 1;
		else if( header.image.pixelDepth == 15 || header.image.pixelDepth == 24 )
			m_numChannels = 3;
		else m_numChannels = 4;

		m_buffer = (uint8_t*)malloc( m_width * m_height * m_numChannels );

		// Skip ID and go to image data/color map data
//		_file.Seek( sizeof(HEADER) + header.colorMap.offset + header.colorMap.length * header.colorMap.depth/8 );

		// Collect some meta information
		bool invertX = (header.image.descriptor & 0x10) == 0x10;
		bool invertY = (header.image.descriptor & 0x20) == 0x20;
		if( header.image.pixelDepth == 16 )
			assert( (header.image.descriptor & 0xf) == 1 );
		else if( header.image.pixelDepth == 32 )
			assert( (header.image.descriptor & 0xf) == 8 );
		else assert( (header.image.descriptor & 0xf) == 0 );
		bool runLengthEncoded = header.imageType > 8;
		int bytePerPixel = (header.image.pixelDepth + 1) / 8;
		int colorDepth = colorMap ? header.colorMap.depth : header.image.pixelDepth;

		// Read image pixel by pixel
		unsigned x = 0, y = 0;
		uint32_t color;		// Buffer value
		int runCount = 0;	// Reuse the last color value?
		bool dataRun = true;// Read value in each pixel
		while( y < m_height )
		{
			while( x < m_width )
			{
				if( runLengthEncoded )
				{
					if( runCount == 0 ) {
						uint8_t control;
						_file.Read( 1, &control );
						runCount = control & 0x7f;
						dataRun = (control & 0x80) == 0;
						goto ReadColor;
					} else --runCount;
				}
				
				if(dataRun)
				{
				ReadColor:
					// Read a single value
					_file.Read( bytePerPixel, &color );
					// Map?
					if( colorMap ) {
						if( bytePerPixel == 2 && !IsLittleEndian() )
							color = ConvertEndian(*(uint16_t*)&color);
						color = *(uint32_t*)(colorMap + color * ((header.colorMap.depth + 1)/8));
					}
				}


				SetColor( (uint8_t*)&color, colorDepth, *this,
					invertX ? (m_width-1-x) : x,
					invertY ? (m_height-1-y) : y );
				++x;
			}
			x = 0;
			++y;
		}
	}

	void ImageWrapper::WriteTGA( IFile& _file ) const
	{
		if( m_numChannels == 2 ) throw "[WriteTGA] Cannot save two channel image into a tga file!";
		if( m_width > 0xffff || m_height > 0xffff ) throw "[WriteTGA] Image too large to be written into a tga file!";

		Header header;
		header.idLength = 0;
		header.colorMapType = 0;
		if( m_numChannels == 1 )
			header.imageType = 3;	// grayscale without runlength
		else header.imageType = 2;	// rgb or rgba without runlength

		header.colorMap.depth = 0;
		header.colorMap.length = 0;
		header.colorMap.offset = 0;

		header.image.xOrigin = 0;
		header.image.yOrigin = 0;
		header.image.width = m_width;
		header.image.height = m_height;
		header.image.pixelDepth = m_numChannels * 8;
		header.image.descriptor = (m_numChannels == 4 ) ? 8 : 0;

		_file.Write( &header, sizeof(Header) );

		// Write data
		for( unsigned y = 0; y < m_height; ++y )
		{
			for( unsigned x = 0; x < m_width; ++x )
			{
				// BGR
				_file.WriteU8( uint8_t(Get(x, y, 2) * 255.0f) );
				_file.WriteU8( uint8_t(Get(x, y, 1) * 255.0f) );
				_file.WriteU8( uint8_t(Get(x, y, 0) * 255.0f) );
				// A
				if( m_numChannels == 4 )
					_file.WriteU8( uint8_t(Get(x, y, 3) * 255.0f) );
			}
		}
	}

} // namespace Files
} // namespace Jo