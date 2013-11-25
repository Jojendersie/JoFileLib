#pragma once

#include <cstdint>
#include <string>
//#include <vector>
//#include <unordered_map>
#include "poolallocator.hpp"

namespace Jo {
namespace Files {

	enum struct Format;
	class IFile;

	/**************************************************************************//**
	 * \class	Jo::Files::MetaFileWrapper
	 * \brief	This class wrapps the structured access to Json and Sraw files.
	 * \details	To use this wrapper in read mode create it with an opened file and
	 *			specify the format. TODO: autodetect. In general it can read all
	 *			sraw and json files, but there is one unsupported json-
	 *			specification: Arrays [] must have values of the same type. The
	 *			usual specification allows different types. The type can still be
	 *			object and each object can contain different values.
	 *
	 *			´MetaFileWrapper Wrapper( someFile, Jo::Files::Format::SRAW )´
	 *****************************************************************************/
	class MetaFileWrapper
	{
		//Format m_Format;
		//const IFile* m_file;
		Memory::PoolAllocator m_nodePool;

	public:
		/// \brief Use a wrapped file to read from.
		/// \details Changing the MetaFileWrapper will not change the input file.
		///		You have to call ´Write´ to do that.
		/// \param _format [in] How should the input be interpreted.
		MetaFileWrapper( const IFile& _file, Format _format );

		/// \brief Clears the old data and loads content from file.
		/// \param _file [in] An opened file which is read. This can also be a
		///		partition of a file where the whole partion must be a valid
		///		meta file of the specified format. The file is not necessarily
		///		read to the end.
		/// \param _format [in] How should the input be interpreted.
		void Read( const IFile& _file, Format _format );

		/// \brief Create an empty wrapper for writing new files.
		/// \details After adding all the data into the wrapper use ´Write´ to
		///		stream the results into a file.
		MetaFileWrapper();

		/// \brief Writes the wrapped data into a file.
		/// \param _file [in] A file opened with write access.
		/// \param _format [in] Format as which the data should be saved.
		void Write( IFile& _file, Format _format ) const;

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

		/// \brief Check if _type is one of the STRINGxx enumeration members.
		/// \return true if _type is STRING8, STRING16, STRING32 or STRING64
		static bool IsStringType( ElementType _type )	{ return _type <= ElementType::STRING64 && _type >= ElementType::STRING8;}

		/// Nodes build a leave oriented tree. Every leave eather contains data or
		/// a refenence to the file where the data is written.
		class Node
		{
			MetaFileWrapper* m_file;
			uint64_t m_numElements;
			ElementType m_type;
			union {
				Node** m_children;			
				//uint64_t m_dataPosition;		///< Position within the file if not buffered
				void* m_bufferArray;			///< Array data is buffered in its own memory block
			};
			mutable uint64_t m_buffer;			///< Primitive non-array data is buffered in that 8 bytes
			mutable uint64_t m_lastAccessed;	///< Array index last used in ´operator[int]´ for optimizations
			std::string m_name;					///< Identifier of the node

			void ParseJson( const IFile& _file );
			void ReadSraw( const IFile& _file );

			/// \brief A node which is returned in case of an access to an
			///		unknown element.
			static Node UndefinedNode;

			/// \brief Creates an empty node of unknown type.
			///
			Node( MetaFileWrapper* _wrapper, const std::string& _name );

			/// \brief Read in a node from file recursively.
			/// \param [in] _wrapper The wrapper with the node pool.
			/// \param [in]
			Node( MetaFileWrapper* _wrapper, const IFile& _file, Format _format );
			void Read( const IFile& _file, Format _format );
			friend class MetaFileWrapper;

			/// \brief No node assignment.
			/// \details If added some time: must be deep copy with setting the
			///		correct wrapper parent...
			void operator = (const Node&);
		public:
			void SaveAsJson( IFile& _file, int _indent=0 ) const;
			void SaveAsSraw( IFile& _file ) const;

			/// \brief Recursive destruction. Assumes all children in the NodePool.
			///
			~Node();

			/// \brief Flat copy construction. The children are just ignored.
			///
			Node( const Node& );

			uint64_t GetNumElements() const		{ return m_numElements; }
			std::string GetName() const			{ return m_name; }
			ElementType GetType() const			{ return m_type; }
			void SetName( const std::string& _name );

			/// \brief Sets type and dimension of the current node.
			/// \details 
			///		Resize operations can delete elements. Elements in the
			///		common range will persist.
			/// \param [in] _size An arbitrary size including 0 (it should fit in memory).
			/// \param [in] _type If the current node type is UNKNOWN this must
			///		be a well defined type. If the node already has a type in
			///		can be UNKNOWN or the type which was set before.
			/// \throws std::string
			void Reset( uint64_t _size, ElementType _type = ElementType::UNKNOWN );

			/// \brief Casts the node data into float.
			/// \details Casting assumes elementary data nodes. If the current
			///		node is array data or an intermediate node the cast will
			///		return garbage! Make sure this is an elementary node before
			///		casting!
			operator float() const				{ return *reinterpret_cast<const float*>(&m_buffer); }

			/// \brief Casts the node data into double.
			/// \details \see{operator float()}
			operator double() const				{ return *reinterpret_cast<const double*>(&m_buffer); }

			/// \brief Casts the node data into signed byte.
			/// \details \see{operator float()}
			operator int8_t() const				{ return *reinterpret_cast<const int8_t*>(&m_buffer); }

			/// \brief Casts the node data into unsigned byte.
			/// \details \see{operator float()}
			operator uint8_t() const			{ return *reinterpret_cast<const uint8_t*>(&m_buffer); }

			operator int16_t() const			{ return *reinterpret_cast<const int16_t*>(&m_buffer); }
			operator uint16_t() const			{ return *reinterpret_cast<const uint16_t*>(&m_buffer); }
			operator int32_t() const			{ return *reinterpret_cast<const int32_t*>(&m_buffer); }
			operator uint32_t() const			{ return *reinterpret_cast<const uint32_t*>(&m_buffer); }
			operator int64_t() const			{ return *reinterpret_cast<const int64_t*>(&m_buffer); }
			operator uint64_t() const			{ return m_buffer; }
			operator bool() const				{ return m_buffer != 0; }

			/// \brief Casts the node data into string.
			/// \details \see{operator float()}
			///
			///		The string is not buffered so this will cause a file access.
			operator std::string() const;

			/// \brief Read in a single value/childnode by name.
			/// \details This method fails for data nodes
			/// \throws std::string
			Node& operator[]( const std::string& _name );
			const Node& operator[]( const std::string& _name ) const;

			/// \brief Read in a single value/childnode by index.
			/// \details This method enlarges the array on out of bounds.
			///		The constant variant will fail.
			///
			///		Accessing more than one time the same index will always
			///		cause a reread - store the value somewhere!
			/// \throws std::string
			Node& operator[]( uint64_t _index );
			const Node& operator[]( uint64_t _index ) const;

			/// \brief Gives direct read / write access to the buffered data.
			/// \details This fails if this is a data node (Type==NODE) or a
			///		string node.
			/// \throws std::string
			void* GetData();

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
			/// \param [in] _name A new which should not be existent in the
			///		current node (not checked).
			/// \param [in] _type The Type of the elementary data. This cannot
			///		be UNKNOWN or NODE if _numElements is != 0.
			/// \param [in] _numElements 0 or a greater number for the array
			///		dimension.
			Node& Add( const std::string& _name, ElementType _type, uint64_t _numElements );

			/// \brief Recurisve recomputation of the size occupied in a sraw file.
			/// \return The new size of this node and all its childs if saved to file.
			uint64_t GetDataSize() const;

			/// \brief Safer access methods with user defined default values.
			///
			float Get( float _default ) const		{ if(m_type == ElementType::FLOAT) return *this; return _default; }
			double Get( double _default ) const		{ if(m_type == ElementType::DOUBLE) return *this; return _default; }
			int8_t Get( int8_t _default ) const		{ if(m_type == ElementType::INT8) return *this; return _default; }
			uint8_t Get( uint8_t _default ) const	{ if(m_type == ElementType::UINT8) return *this; return _default; }
			int16_t Get( int16_t _default ) const	{ if(m_type == ElementType::INT16) return *this; return _default; }
			uint16_t Get( uint16_t _default ) const	{ if(m_type == ElementType::UINT16) return *this; return _default; }
			int32_t Get( int32_t _default ) const	{ if(m_type == ElementType::INT32) return *this; return _default; }
			uint32_t Get( uint32_t _default ) const	{ if(m_type == ElementType::UINT32) return *this; return _default; }
			int64_t Get( int64_t _default ) const	{ if(m_type == ElementType::INT64) return *this; return _default; }
			uint64_t Get( uint64_t _default ) const	{ if(m_type == ElementType::UINT64) return *this; return _default; }
			bool Get( bool _default ) const			{ if(m_type == ElementType::BIT) return *this; return _default; }
		};

		Node RootNode;

		/// \brief Direct access to the root node. See Node::operator[] for
		///		more details.
		Node& operator[]( const std::string& _name )				{ return RootNode[_name]; }
		const Node& operator[]( const std::string& _name ) const	{ return RootNode[_name]; }

		/// \brief Direct access to the root node. See Node::operator[] for
		///		more details.
		Node& operator[]( uint64_t _index )							{ return RootNode[_index]; }
		const Node& operator[]( uint64_t _index ) const				{ return RootNode[_index]; }
	};

};
};