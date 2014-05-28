#include "imagewrapper.hpp"
#include "jofilelib.hpp"
#include <algorithm>

// TODO: padding (if bitdepth < 8)!

namespace Jo {
namespace Files {

	ImageWrapper::ImageWrapper( const IFile& _file, Format _format ) :
		m_buffer(nullptr)
	{
		// Choose correct load method
		switch(_format)
		{
		case Format::PNG: ReadPNG( _file ); break;
		case Format::PFM: ReadPFM( _file ); break;
		default:
			assert(false);
			throw std::string("[ImageWrapper::ImageWrapper] Cannot open non-image format.");
		}
	}

	ImageWrapper::ImageWrapper( uint32_t _width, uint32_t _height, uint32_t _numChannels, ChannelType _type, int _bitDepth ) :
		m_width(_width),
		m_height(_height),
		m_numChannels(_numChannels),
		m_bitDepth(_bitDepth),
		m_channelType(_type)
	{
		if( _type == ChannelType::FLOAT )
			m_bitDepth = 32;
		// Round up in case the last byte is not filled completely.
		m_buffer = (uint8_t*)malloc( (m_width * m_height * m_numChannels * m_bitDepth + 7) / 8 );
	}

	ImageWrapper::~ImageWrapper()
	{
		free( m_buffer );
	}


	void ImageWrapper::Write( IFile& _file, Format _format ) const
	{
		// Choose correct write method
		switch(_format)
		{
		case Format::PNG: WritePNG( _file ); break;
		case Format::PFM: WritePFM( _file ); break;
		default:
			assert(false);
			throw std::string("[ImageWrapper::Write] Target format not supported.");
		}
	}

	void ImageWrapper::Set( int _x, int _y, int _c, float _value )
	{
		// Do nothing if out of boundaries
		if( _x < 0 || _x >= (int)m_width || _y < 0 || _y >= (int)m_height || _c < 0 || _c >= (int)m_numChannels )
			return;

		switch(m_channelType)
		{
		case ChannelType::UINT: {
			// Clamp to [0,1] then convert to uint with correct bit depth
			_value = std::min(1.0f,std::max(0.0f, _value ));
			uint32_t bitOffset = m_bitDepth * ((_x + _y * m_width)*m_numChannels + _c);
			void* offset = m_buffer + bitOffset / 8;
			bitOffset = bitOffset % 8;
			switch (m_bitDepth)
			{
				case 1:  *(uint8_t*)offset  = (*(uint8_t*)offset) & ~(0x80 >> bitOffset) | (uint8_t(_value) << (7-bitOffset)); break;
				case 2:  *(uint8_t*)offset  = (*(uint8_t*)offset) & ~(0xc0 >> bitOffset) | (uint8_t(_value * 3.0f) << (6-bitOffset)); break;
				case 4:  *(uint8_t*)offset  = (*(uint8_t*)offset) & (0xf << bitOffset) | (uint8_t(_value * 15.0f) << (4-bitOffset)); break;
				case 8:  *(uint8_t*)offset  = uint8_t(_value * 255.0f); break;
				case 16: *(uint16_t*)offset = uint16_t(_value * 65535.0f); break;
				case 32: *(uint32_t*)offset = uint32_t(_value * 4294967296.0); break;
			}
			} break;
		case ChannelType::INT: {
			// Clamp to [0,1] then convert to int with correct bit depth
			_value = std::min(1.0f,std::max(-1.0f, _value ));
			assert(m_bitDepth >= 8);
			void* offset = m_buffer + m_bitDepth * ((_x + _y * m_width)*m_numChannels + _c) / 8;
			switch (m_bitDepth)
			{
				case 8:  *(int8_t*)offset  = int8_t(_value * 127.5f - 0.5f); break;
				case 16: *(int16_t*)offset = int16_t(_value * 32767.5f - 0.5f); break;
				case 32: *(int32_t*)offset = int32_t(_value * 2147483648.0 - 0.5); break;
			}
		} break;
		case ChannelType::FLOAT: {
			assert(m_bitDepth == 32);
			// Just set at the correct buffer position
			float* offset = (float*)(m_buffer + 4 * ((_x + _y * m_width) * m_numChannels + _c));
			*offset = _value;
			} break;
		}
	}


	float ImageWrapper::Get( int _x, int _y, int _c )
	{
		// Do nothing if out of boundaries
		if( _x < 0 || _x >= (int)m_width || _y < 0 || _y >= (int)m_height || _c < 0 || _c >= (int)m_numChannels )
			return -10000.0f;

		switch(m_channelType)
		{
		case ChannelType::UINT: {
			// Extract the correct bits
			uint32_t bitOffset = m_bitDepth * ((_x + _y * m_width)*m_numChannels + _c);
			void* offset = m_buffer + bitOffset / 8;
			bitOffset = bitOffset % 8;
			switch (m_bitDepth)
			{
				case 1:  return float((*(uint8_t*)offset) & (0x80 >> bitOffset));
				case 2:  return ((*(uint8_t*)offset) & (0xc0 >> bitOffset)) / 3.0f;
				case 4:  return ((*(uint8_t*)offset) & (0xf0 >> bitOffset)) / 15.0f;
				case 8:  return (*(uint8_t*)offset) / 255.0f;
				case 16: return (*(uint16_t*)offset) / 65535.0f;
				case 32: return float((*(uint32_t*)offset) / 4294967296.0);
			}
			} break;
		case ChannelType::INT: {
			// Convert from int to float
			assert(m_bitDepth >= 8);
			void* offset = m_buffer + m_bitDepth * ((_x + _y * m_width)*m_numChannels + _c) / 8;
			switch (m_bitDepth)
			{
				case 8:  return ((*(int8_t*)offset)  + 0.5f) / 127.5f;
				case 16: return ((*(int16_t*)offset) + 0.5f) / 32767.5f;
				case 32: return float(((*(int32_t*)offset) + 0.5 ) / 2147483648.0);
			}
		} break;
		case ChannelType::FLOAT: {
			assert(m_bitDepth == 32);
			// Just get at the correct buffer position
			float* offset = (float*)(m_buffer + 4 * ((_x + _y * m_width) * m_numChannels + _c));
			return *offset;
			} break;
		}
		return 0.0f;
	}

} // namespace Files
} // namespace Jo