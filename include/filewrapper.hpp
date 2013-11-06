#pragma once

#include <cstdint>
#include <string>
//#include <vector>
//#include <unordered_map>
#include <poolallocator.hpp>

namespace Jo {
namespace Files {

	enum struct Format;
	class IFile;

	/**************************************************************************//**
	 * \class	Jo::Files::JsonSrawWrapper
	 * \brief	This class wrapps the structured access to Json and Sraw files.
	 * \details	To use this wrapper in read mode create it with an opened file and
	 *			specify the format. TODO: autodetect. In general it can read all
	 *			sraw and json files, but there is one unsupported json-
	 *			specification: Arrays [] must have values of the same type. The
	 *			usual specification allows different types. The type can still be
	 *			object and each object can contain different values.
	 *
	 *			´JsonSrawWrapper Wrapper( someFile, Jo::Files::Format::SRAW )´
	 *****************************************************************************/
	class JsonSrawWrapper
	{
		//Format m_Format;
		//const IFile* m_pFile;
		Memory::PoolAllocator m_pNodePool;

	public:
		/// \brief Use a wrapped file to read from.
		/// \details Changing the JsonSrawWrapper will not change the input file.
		///		You have to call ´Write´ to do that.
		/// \param _Format [in] How should the input be interpreted.
		JsonSrawWrapper( const IFile& _File, Format _Format );

		/// \brief Create an empty wrapper for writing new files.
		/// \details After adding all the data into the wrapper use ´Write´ to
		///		stream the results into a file.
		JsonSrawWrapper();

		/// \brief Writes the wrapped data into a file.
		/// \param _File [in] A file opened with write access.
		/// \param _Format [in] Format as which the data should be saved.
		void Write( IFile& _File, Format _Format ) const;

		enum struct ElementType
		{
			NODE		= 0x0,
			STRING8		= 0x1,
			STRING16	= 0x2,
			STRING32	= 0x3,
			STRING64	= 0x4,
			BIT			= 0x5,
			INT8		= 0x6,
			UINT8		= 0x7,
			INT16		= 0x8,
			UINT16		= 0x9,
			INT32		= 0xa,
			UINT32		= 0xb,
			INT64		= 0xc,
			UINT64		= 0xd,
			FLOAT		= 0xe,
			DOUBLE		= 0xf,
			UNKNOWN		= 0x10
		};

		static const int64_t ELEMENT_TYPE_SIZE[];

		/// \brief Check if _Type is one of the STRINGxx enumeration members.
		/// \return true if _Type is STRING8, STRING16, STRING32 or STRING64
		static bool IsStringType( ElementType _Type )	{ return _Type <= ElementType::STRING64 && _Type >= ElementType::STRING8;}

		/// Nodes build a leave oriented tree. Every leave eather contains data or
		/// a refenence to the file where the data is written.
		class Node
		{
			JsonSrawWrapper* m_pFile;
			uint64_t m_iNumElements;
			ElementType m_Type;
			union {
				Node** m_pChildren;			
				//uint64_t m_iDataPosition;		///< Position within the file if not buffered
				void* m_pBuffer;				///< Array data is buffered in its own memory block
			};
			mutable uint64_t m_iBuffer;			///< Primitive non-array data is buffered in that 8 bytes
			mutable uint64_t m_iLastAccessed;	///< Array index last used in ´operator[int]´ for optimizations
			std::string m_Name;					///< Identifier of the node

			void ParseJson( const IFile& _File );
			void ReadSraw( const IFile& _File );

			/// \brief A node which is returned in case of an access to an
			///		unknown element.
			static Node UndefinedNode;

			/// \brief Creates an empty node of unknown type.
			///
			Node( JsonSrawWrapper* _pWrapper, const std::string& _Name );

			/// \brief Read in a node from file recursively.
			/// \param [in] _pWrapper The wrapper with the node pool.
			/// \param [in]
			Node( JsonSrawWrapper* _pWrapper, const IFile& _File, Format _Format );
			friend class JsonSrawWrapper;

			/// \brief No node assignment.
			/// \details If added some time: must be deep copy with setting the
			///		correct wrapper parent...
			void operator = (const Node&);
		public:
			void SaveAsJson( IFile& _File ) const;
			void SaveAsSraw( IFile& _File ) const;

			/// \brief Recursive destruction. Assumes all children in the NodePool.
			///
			~Node();

			/// \brief Flat copy construction. The children are just ignored.
			///
			Node( const Node& );

			uint64_t GetNumElements() const		{ return m_iNumElements; }
			std::string GetName() const			{ return m_Name; }
			ElementType GetType() const			{ return m_Type; }
			void SetName( const std::string& _Name );

			/// \brief Casts the node data into float.
			/// \details Casting assumes elementary data nodes. If the current
			///		node is array data or an intermediate node the cast will
			///		return garbage! Make sure this is an elementary node before
			///		casting!
			operator float() const				{ return *reinterpret_cast<const float*>(&m_iBuffer); }

			/// \brief Casts the node data into double.
			/// \details \see{operator float()}
			operator double() const				{ return *reinterpret_cast<const double*>(&m_iBuffer); }

			/// \brief Casts the node data into signed byte.
			/// \details \see{operator float()}
			operator int8_t() const				{ return *reinterpret_cast<const int8_t*>(&m_iBuffer); }

			/// \brief Casts the node data into unsigned byte.
			/// \details \see{operator float()}
			operator uint8_t() const			{ return *reinterpret_cast<const uint8_t*>(&m_iBuffer); }

			operator int16_t() const			{ return *reinterpret_cast<const int16_t*>(&m_iBuffer); }
			operator uint16_t() const			{ return *reinterpret_cast<const uint16_t*>(&m_iBuffer); }
			operator int32_t() const			{ return *reinterpret_cast<const int32_t*>(&m_iBuffer); }
			operator uint32_t() const			{ return *reinterpret_cast<const uint32_t*>(&m_iBuffer); }
			operator int64_t() const			{ return *reinterpret_cast<const int64_t*>(&m_iBuffer); }
			operator uint64_t() const			{ return m_iBuffer; }
			operator bool() const				{ return m_iBuffer != 0; }

			/// \brief Casts the node data into string.
			/// \details \see{operator float()}
			///
			///		The string is not buffered so this will cause a file access.
			operator std::string() const;

			/// \brief Read in a single value/childnode by name.
			/// \details This method fails for data nodes
			Node& operator[]( const std::string& _Name ) throw(std::string);
			const Node& operator[]( const std::string& _Name ) const throw(std::string);

			/// \brief Read in a single value/childnode by index.
			/// \details This method enlarges the array on out of bounds.
			///		The constant variant will fail.
			///
			///		Accessing more than one time the same index will always
			///		cause a reread - store the value somewhere!
			Node& operator[]( uint64_t _iIndex ) throw(std::string);
			const Node& operator[]( uint64_t _iIndex ) const throw(std::string);

			/// \brief Gives direct read / write access to the buffered data.
			/// \details This fails if this is a data node (Type==NODE) or a
			///		string node.
			void* GetData() throw(std::string);

			float operator = (float _val);
			double operator = (double _val);
			int8_t operator = (int8_t _val);
			uint8_t operator = (uint8_t _val);
			int16_t operator = (int16_t _val);
			uint16_t operator = (uint16_t _val);
			int32_t operator = (int32_t _val);
			uint32_t operator = (uint32_t _val);
			int64_t operator = (int64_t _val);
			uint64_t operator = (uint64_t _val);
			bool operator = (bool _val);
			const std::string& operator = (const std::string& _val);

			/// \brief Create a subnode with an array of elementary type.
			/// \param [in] _Name A new which should not be existent in the
			///		current node (not checked).
			/// \param [in] _Type The Type of the elementary data. This cannot
			///		be UNKNOWN or NODE if _iNumElements is != 0.
			/// \param [in] _iNumElements 0 or a greater number for the array
			///		dimension.
			Node& Add( const std::string& _Name, ElementType _Type, uint64_t _iNumElements );

			/// \brief Recurisve recomputation of the size occupied in a sraw file.
			/// \return The new size of this node and all its childs if saved to file.
			uint64_t GetDataSize() const;

			/// \brief Safer access methods with user defined default values.
			///
			float Get( float _Default ) const		{ if(m_Type == ElementType::FLOAT) return *this; return _Default; }
			double Get( double _Default ) const		{ if(m_Type == ElementType::DOUBLE) return *this; return _Default; }
			int8_t Get( int8_t _Default ) const		{ if(m_Type == ElementType::INT8) return *this; return _Default; }
			uint8_t Get( uint8_t _Default ) const	{ if(m_Type == ElementType::UINT8) return *this; return _Default; }
			int16_t Get( int16_t _Default ) const	{ if(m_Type == ElementType::INT16) return *this; return _Default; }
			uint16_t Get( uint16_t _Default ) const	{ if(m_Type == ElementType::UINT16) return *this; return _Default; }
			int32_t Get( int32_t _Default ) const	{ if(m_Type == ElementType::INT32) return *this; return _Default; }
			uint32_t Get( uint32_t _Default ) const	{ if(m_Type == ElementType::UINT32) return *this; return _Default; }
			int64_t Get( int64_t _Default ) const	{ if(m_Type == ElementType::INT64) return *this; return _Default; }
			uint64_t Get( uint64_t _Default ) const	{ if(m_Type == ElementType::UINT64) return *this; return _Default; }
			bool Get( bool _Default ) const			{ if(m_Type == ElementType::BIT) return *this; return _Default; }
		};

		Node RootNode;
	};

};
};