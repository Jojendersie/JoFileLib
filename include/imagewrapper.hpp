#pragma once

#include <cstdint>

namespace Jo {
namespace Files {

	enum struct Format;
	class IFile;

	/**************************************************************************//**
	 * \class	Jo::Files::ImageWrapper
	 * \brief	Loads images of various types and provides a unfied access.
	 * \details	In general color palettes are not supported!
	 *****************************************************************************/
	class ImageWrapper
	{
	public:
		/// \brief Use a wrapped file to read from.
		/// \details This is only to read images call write to save an image.
		/// \param _format [in] How should the input be interpreted.
		ImageWrapper( const IFile& _file, Format _format );

		~ImageWrapper();

		/// \brief Return the width of the image in memory in pixels.
		uint32_t Width() const		{ return m_width; }

		/// \brief Return the height of the image in memory in pixels.
		uint32_t Height() const		{ return m_height; }

		/// \brief Return the number of different channels (e.g. RGB -> 3).
		uint32_t NumChannels() const	{ return m_numChannels; }

		enum struct ChannelType {
			UINT,	///< Any unsigned int with 1,2,4,8,16 or 32 bit (depends on bitdepth)
			INT,	///< Any signed int with 8, 16 or 32 bit (depends on bitdepth)
			FLOAT	///< IEEE754 32bit float
		};

		/// \brief Return the type of all channels (they all have the same)
		ChannelType GetChannelType() const	{ return m_channelType; }

		/// \brief Return number of bits per channel.
		int BitDepth() const 				{ return m_bitDepth; }

		/// \brief Direct access to the image memory.
		/// \details This should be used for loading up images to the GPU.
		///
		///		The pixels are stored row wise.
		const void* GetBuffer() const		{ return m_buffer; }
		
		/// \brief Set one channel of a pixel.
		/// \details If the format is not float the value will be converted.
		///		Therefore the value is clamped to [0,1] or [-1,1]. It is not
		///		clamped if the channel contains floats!
		void Set( int _x, int _y, int _c, float _value );

		/// \brief Get one channel of a pixel.
		/// \details If the format is not float the value will be converted.
		/// \return Value in [0,1] if unsigned channels, [-1,1] for signed int
		///		or unconverted float value.
		float Get( int _x, int _y, int _c );

		/// \brief Write image to a file of arbitrary type.
		/// \details This method fails if the target format does not support
		///		the current channels + bitdepth
		void Write( IFile& _file, Format _format ) const;

	private:
		uint8_t* m_buffer;		///< RAW buffer of all pixels (row major)
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_numChannels;
		int m_bitDepth;
		ChannelType m_channelType;
		int m_pixelSize;		///< Size of a pixel in bytes (= m_numChannels * m_bitDepth / 8)

		void ReadPNG( const IFile& _file );
		void WritePNG( IFile& _file ) const;
	};
};
};