#include "jofilelib.hpp"
#include "file.hpp"
#include "filewrapper.hpp"
#include <cctype>
#include <string>
using namespace std; 

namespace Jo {
namespace Files {

	const int64_t MetaFileWrapper::ELEMENT_TYPE_SIZE[] = { sizeof(Node*)*8, sizeof(std::string)*8, -1, -1, -1, 1, 8, 16, 32, 64, 8, 16, 32, 64, 32, 64 };
	static int NELEM_SIZE(uint8_t _code) { return 1<<((_code & 0x30)>>4); }

	/// \brief Calculate the space required by the bufferArray
#define ARRAY_SIZE(n,T)		(((n) * MetaFileWrapper::ELEMENT_TYPE_SIZE[(int)(T)] + 7) / 8)


	MetaFileWrapper::Node MetaFileWrapper::Node::UndefinedNode( nullptr, std::string() );

	// ********************************************************************* //
	// Static helper methods												 //
	// ********************************************************************* //

	static char FindFirstNonWhitespace( const IFile& _file )
	{
		char charBuffer;
		if( _file.IsEof() ) throw std::string("Syntax error in json file. Unexpected end of file.");
		charBuffer = _file.Next();
		while( std::isspace(charBuffer) )
		{
			if( _file.IsEof() ) throw std::string("Syntax error in json file. Unexpected end of file.");
			charBuffer = _file.Next();
		}
		return charBuffer;
	}
	static std::string ReadJsonIdentifier( const IFile& _file )
	{
		std::string identifier("");
		int index = 0;
		char previous;
		do {
			char charBuffer = 0;
			do {
				previous = charBuffer;
				if( _file.IsEof() ) throw std::string("Syntax error in json file. Unexpected end of file.");
				charBuffer = _file.Next();
				identifier += charBuffer;
			} while( charBuffer != '"' );
			// Found a ". It could be escaped -> repeat.
		} while( previous != 0 && previous=='\\' );

		identifier.pop_back();
		return identifier;
	}
	static std::string ReadJsonNumber( const IFile& _file, bool& _isFloat )
	{
		// Assume integer numbers
		_isFloat = false;
		std::string number("");
		int index = 0;
		char charBuffer = 0;
		do {
			if( _file.IsEof() ) throw std::string("Syntax error in json file. Unexpected end of file.");
			// Read one character and append
			charBuffer = _file.Next();
			number += charBuffer;
			// It is a float!
			if( charBuffer == '.' || charBuffer == 'e') _isFloat = true;
			// Silently accept any delimiting character.
		} while( charBuffer != ',' && charBuffer != '\n' && charBuffer != '}' && charBuffer != ']' );

		number.pop_back();
		_file.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
		return number.c_str();
	}

	// ********************************************************************* //
	// Determine the minimum variable size to store the value in _iVal
	// as a power of two.
	static int GetNumRequiredBytes( uint64_t _iVal )
	{
		if( _iVal > 0xffffffff ) return 3;
		else if( _iVal > 0xffff ) return 2;
		else if( _iVal > 0xff ) return 1;
		else return 0;
	}

	// ********************************************************************* //
	// Read a string with variable sized length header from file to std::string
	static void ReadString( const IFile& _file, int _stringSize, std::string& _Out )
	{
		uint64_t length = 0;
		_file.Read( _stringSize, &length );
		_Out.resize( size_t(length) );
		_file.Read( length, &_Out[0] );
	}

	// ********************************************************************* //
	// MetaFileWrapper														 //
	// ********************************************************************* //

	// ********************************************************************* //
	// Use a wrapped file to read from.
	MetaFileWrapper::MetaFileWrapper( const IFile& _file, Format _format ) :
		m_nodePool(sizeof(Node)),
		RootNode(this, _file, _format)
	{
	}

	// ********************************************************************* //
	// Clears the old data and loads content from file.
	void MetaFileWrapper::Read( const IFile& _file, Format _format )
	{
		RootNode.~Node();
		// After the ~Node the following call should do nothing
		m_nodePool.FreeAll();

		// Load from file
		RootNode.Read( _file, _format );
	}

	// ********************************************************************* //
	// Create an empty wrapper for writing new files.
	MetaFileWrapper::MetaFileWrapper() :
		m_nodePool(sizeof(Node)),
		RootNode(this, "Root")
	{
	}

	// ********************************************************************* //
	// Writes the wrapped data into a file.
	void MetaFileWrapper::Write( IFile& _file, Format _format ) const
	{
		if( _format == Format::JSON ) 
			RootNode.SaveAsJson( _file );
		else RootNode.SaveAsSraw( _file );
	}



	// ********************************************************************* //
	// MetaFileWrapper::Node												 //
	// ********************************************************************* //

	// ********************************************************************* //
	MetaFileWrapper::Node::Node( MetaFileWrapper* _wrapper, const std::string& _name ) :
		m_file( _wrapper ),
		m_numElements( 0 ),
		m_type( ElementType::UNKNOWN ),
		m_bufferArray( m_buffer ),
		m_lastAccessed( 0 ),
		m_name( _name )
	{
	}

	// ********************************************************************* //
	MetaFileWrapper::Node::Node( MetaFileWrapper* _wrapper, const IFile& _file, Format _format ) :
		m_file( _wrapper ),
		m_numElements( 0 ),
		m_type( ElementType::UNKNOWN ),
		m_bufferArray( m_buffer ),
		m_lastAccessed( 0 ),
		m_name("")
	{
		// Ignore empty files
		if( !_file.IsEof() )
			Read( _file, _format );
	}

	// ********************************************************************* //
	MetaFileWrapper::Node::~Node()
	{
		if( m_file )	// Only if this is not a flat copy
		{
			if( m_type == ElementType::NODE )
			{
				for( uint64_t i=0; i<m_numElements; ++i )
					m_file->m_nodePool.Delete( ((Node**)m_bufferArray)[i] );
			} else if( m_type == ElementType::STRING )
			{
				for( uint64_t i=0; i<m_numElements; ++i )
					((string*)m_bufferArray+i)->~string();
			}

			if( ARRAY_SIZE(m_numElements, m_type) > sizeof(m_buffer) )
				free(m_bufferArray);
		}
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::Read( const IFile& _file, Format _format )
	{
		// Read in the first few bytes to test which format it is.
		// In json files either {" or {} are valid (expanded with white spaces).
		// if these two symbols can be found the format is assumed to be json.
		if( _format == Format::AUTO_DETECT )
		{
			_format = Format::SRAW;	// Default if nothing else can be detected
			try {
				char charBuffer = FindFirstNonWhitespace(_file);
				if( charBuffer == '{' ) {
					charBuffer = FindFirstNonWhitespace(_file);
					if( charBuffer == '}' || charBuffer == '"' )
						_format = Format::JSON;
				}
			} catch(...) {}
			// Let the parser see everything
			_file.Seek( 0 );
		}
		if( _format == Format::JSON ) 
			ParseJson( _file );
		else ReadSraw( _file );
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::ParseJsonValue( const IFile& _file, char _fistNonWhite )
	{
		// What type has the value?
		switch(_fistNonWhite) {
		case '{':
			// Use recursion therefore the object must start with { -> go back
			_file.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
			ParseJson( _file );
			break;
		case '"':
			// This is a string
			*this = ReadJsonIdentifier( _file );
			break;
		case '[':
			// Go into recursion
			ParseJsonArray(_file);
			break;
		case 't':
			// boolean value "true"
			_file.Seek( 3, IFile::SeekMode::MOVE_FORWARD );
			*this = true;
			break;
		case 'f':
			// boolean value "false"
			_file.Seek( 4, IFile::SeekMode::MOVE_FORWARD );
			*this = false;
			break;
		case 'n':
			// Skip null reference -> node remains untyped
			_file.Seek( 3, IFile::SeekMode::MOVE_FORWARD );
			break;
		}

		// Check for number types
		if( _fistNonWhite >= '0' && _fistNonWhite <= '9' || _fistNonWhite == '-' )
		{
			// Parse number
			bool isFloat;
			std::string number = _fistNonWhite + ReadJsonNumber( _file, isFloat );
			if( isFloat ) {
				*this = atof(number.c_str());
			} else {
				// Using strtoul allows reading numbers like 0xffffffff
				*this = (int)strtoul(number.c_str(), nullptr, 0);
			}
		}
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::ParseJsonArray( const IFile& _file )
	{
		// Expects the '[' as already read.

		// Now there are values or the end of the array.
		char charBuffer = FindFirstNonWhitespace(_file);
		uint64_t index = 0;
		while( charBuffer != ']' )
		{
			// Most values are read plane into the array except other arrays
			// or objects which require a new node.
			if( charBuffer == '[' || charBuffer == '{' )
			{
				if(index != 0 && m_type!=ElementType::NODE) throw std::string("[Node::ParseJsonArray] Arrays must have the same type everywhere!");
				m_type = ElementType::NODE;
				(*this)[index].ParseJsonValue(_file, charBuffer);
			} else {
				if(index==0) ParseJsonValue(_file, charBuffer);
				else (*this)[index].ParseJsonValue(_file, charBuffer);
			}
			++index;
			charBuffer = FindFirstNonWhitespace(_file);
			if( charBuffer != ',' && charBuffer != ']' )
				throw std::string("Syntax error in json file. Expected , or ]");
			// There is another value
			if( charBuffer == ',' )
				charBuffer = FindFirstNonWhitespace(_file);
		}
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::ParseJson( const IFile& _file )
	{
		// First step search opening '{'
		char charBuffer = FindFirstNonWhitespace(_file);
		if( charBuffer != '{' ) throw std::string("Syntax error in json file. Expected {");
		m_type = ElementType::NODE;

		do {
			// Now we are inside the root node search the first identifier
			charBuffer = FindFirstNonWhitespace(_file);
			if( charBuffer == '}' ) break;	// There was one, too much: try to continue with the assumption of the object end.
			if( charBuffer != '"' ) throw std::string("Syntax error in json file. Expected \"");
			std::string identifier = ReadJsonIdentifier( _file );

			// Now there must be a :
			charBuffer = FindFirstNonWhitespace(_file);
			if( charBuffer != ':' ) throw std::string("Syntax error in json file. Expected :");

			Node& newNode = Add( identifier, ElementType::UNKNOWN, 0 );
			newNode.ParseJsonValue(_file, FindFirstNonWhitespace(_file));
			
			charBuffer = FindFirstNonWhitespace(_file);
		} while(charBuffer == ',');
		if( charBuffer != '}' ) throw std::string("Syntax error in json file. Object must end with }");
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::ReadSraw( const IFile& _file )
	{
		// The first byte has CODE ELEM_TYPE nibbles
		uint8_t codeNType = _file.Next();
		m_type = (ElementType)(codeNType & 0xf);

		// Map all STRINGxx types to STRING but remember the size for ReadString.
		// The number is useless for non string types
		int stringSize = 1;
		if( (int)m_type <= 0x4 && m_type > ElementType::STRING ) {
			stringSize = 1<<((int)m_type-(int)MetaFileWrapper::ElementType::STRING);
			m_type = ElementType::STRING;
		}

		// Then the identifier follows as STRING8
		ReadString( _file, 1, m_name );

		// Read NELEMS (array dimension)
		uint64_t numElements = 0;
		_file.Read( NELEM_SIZE(codeNType), &numElements );

		// Read or calculate the data block size (could be used to skip blocks too)
		uint64_t dataSize;
		if( m_type == ElementType::NODE || m_type == ElementType::STRING )
			_file.Read( 8, &dataSize );
		else
			dataSize = ARRAY_SIZE(numElements, m_type);

		Resize( numElements );

		if( m_type == ElementType::NODE )
		{
			// Recursive read (file cursor is at the correct position).
			for( uint64_t i=0; i<m_numElements; ++i )
			{
				((Node**)m_bufferArray)[i]->ReadSraw( _file );
			}
		} else {
			// Now the files cursor is at the beginning of the data
			if( m_type == ElementType::STRING )
			{
				// Buffer single string objects
				for( uint64_t i=0; i<m_numElements; ++i )
					ReadString( _file, stringSize, ((std::string*)m_bufferArray)[i] );
			} else if( m_numElements > 0 ) {
				_file.Read( dataSize, m_bufferArray );
			}
		}
	}


	// ********************************************************************* //
	float MetaFileWrapper::Node::Get( float _default ) const
	{
		switch(m_type)
		{
		case ElementType::FLOAT:
			return (float)*this;
		case ElementType::DOUBLE:
			return float((double)*this);
		default:
			return _default;
		}
	}

	double MetaFileWrapper::Node::Get( double _default ) const
	{
		switch(m_type)
		{
		case ElementType::FLOAT:
			return (float)*this;
		case ElementType::DOUBLE:
			return (double)*this;
		default:
			return _default;
		}
	}

	int8_t MetaFileWrapper::Node::Get( int8_t _default ) const
	{
		switch(m_type)
		{
		case Jo::Files::MetaFileWrapper::ElementType::INT8:
			return int8_t((int8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT8:
			return int8_t((uint8_t)*this);
		default:
			return _default;
		}
	}

	uint8_t MetaFileWrapper::Node::Get( uint8_t _default ) const
	{
		switch(m_type)
		{
		case Jo::Files::MetaFileWrapper::ElementType::INT8:
			return uint8_t((int8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT8:
			return uint8_t((uint8_t)*this);
		default:
			return _default;
		}
	}

	int16_t MetaFileWrapper::Node::Get( int16_t _default ) const
	{
		switch(m_type)
		{
		case Jo::Files::MetaFileWrapper::ElementType::INT8:
			return int16_t((int8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT16:
			return int16_t((int16_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT8:
			return int16_t((uint8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT16:
			return int16_t((uint16_t)*this);
		default:
			return _default;
		}
	}

	uint16_t MetaFileWrapper::Node::Get( uint16_t _default ) const
	{
		switch(m_type)
		{
		case Jo::Files::MetaFileWrapper::ElementType::INT8:
			return uint16_t((int8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT16:
			return uint16_t((int16_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT8:
			return uint16_t((uint8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT16:
			return uint16_t((uint16_t)*this);
		default:
			return _default;
		}
	}

	int32_t MetaFileWrapper::Node::Get( int32_t _default ) const
	{
		switch(m_type)
		{
		case Jo::Files::MetaFileWrapper::ElementType::INT8:
			return int32_t((int8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT16:
			return int32_t((int16_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT32:
			return int32_t((int32_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT8:
			return int32_t((uint8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT16:
			return int32_t((uint16_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT32:
			return int32_t((uint32_t)*this);
		default:
			return _default;
		}
	}

	uint32_t MetaFileWrapper::Node::Get( uint32_t _default ) const
	{
		switch(m_type)
		{
		case Jo::Files::MetaFileWrapper::ElementType::INT8:
			return uint32_t((int8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT16:
			return uint32_t((int16_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT32:
			return uint32_t((int32_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT8:
			return uint32_t((uint8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT16:
			return uint32_t((uint16_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT32:
			return uint32_t((uint32_t)*this);
		default:
			return _default;
		}
	}

	int64_t MetaFileWrapper::Node::Get( int64_t _default ) const
	{
		switch(m_type)
		{
		case Jo::Files::MetaFileWrapper::ElementType::INT8:
			return int64_t((int8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT16:
			return int64_t((int16_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT32:
			return int64_t((int32_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT64:
			return int64_t((int64_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT8:
			return int64_t((uint8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT16:
			return int64_t((uint16_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT32:
			return int64_t((uint32_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT64:
			return int64_t((uint64_t)*this);
		default:
			return _default;
		}
	}

	uint64_t MetaFileWrapper::Node::Get( uint64_t _default ) const
	{
		switch(m_type)
		{
		case Jo::Files::MetaFileWrapper::ElementType::INT8:
			return uint64_t((int8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT16:
			return uint64_t((int16_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT32:
			return uint64_t((int32_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::INT64:
			return uint64_t((int64_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT8:
			return uint64_t((uint8_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT16:
			return uint64_t((uint16_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT32:
			return uint64_t((uint32_t)*this);
		case Jo::Files::MetaFileWrapper::ElementType::UINT64:
			return uint64_t((uint64_t)*this);
		default:
			return _default;
		}
	}


	// ********************************************************************* //
	void MetaFileWrapper::Node::SaveAsJson( IFile& _file, int _indent ) const
	{
		// Do not save unknown garbage
		if( m_type == ElementType::UNKNOWN ) return;

		std::string buffer;
		// Start with indent + identifier
		if( _indent != 0 && m_name != "" )	// Not for root node or unnamed nodes
		{
			buffer = "";
			for( int i=0; i<_indent; ++i )
				buffer += ' ';
			// "Name": 
			buffer += '\"' + m_name + "\": ";
			_file.Write( buffer.c_str(), buffer.length() );
		} else for( int i=0; i<_indent; ++i ) _file.Write( " ", 1 );

		// Add nodes recursively
		if( m_type == ElementType::NODE )
		{
			// Node arrays? Look if there is a child without name.
			bool nodeArray = (m_numElements==0) || (*this)[0].m_name == "";
			if( nodeArray )	_file.Write( "[\n", 2 );
			else _file.Write( "{\n", 2 );
			for( uint64_t i=0; i<m_numElements; ++i )
			{
				(*this)[i].SaveAsJson( _file, _indent+2 );
				// All variables are delimited by ,
				if(i+1<m_numElements) _file.Write( ",\n", 2 );
				else _file.Write( "\n", 1 );
			}
			for( int i=0; i<_indent; ++i ) _file.Write( " ", 1 );
			if( nodeArray )	_file.Write( "]", 1 );
			else _file.Write( "}", 1 );
		} else {
			// Now data is coming
			// If there is more than one element add array syntax []
			// Also use [] for empty data arrays.
			int subIndent = 0;
			// This is a child node of an array -> Array of value-arrays
			bool isArray = (m_name == "") || ( m_numElements != 1 );
			if(isArray) _file.Write( "[", 1 );
			for( uint64_t i=0; i<m_numElements; ++i )
			{
				switch( m_type )
				{
				case ElementType::BIT:		if((*this)[i]) buffer = "true"; else buffer = "false";	break;
				case ElementType::DOUBLE:	buffer = std::to_string( double((*this)[i]) );			break;
				case ElementType::FLOAT:	buffer = std::to_string( float((*this)[i]) );			break;
				case ElementType::INT8:		buffer = std::to_string( int8_t((*this)[i]) );			break;
				case ElementType::INT16:	buffer = std::to_string( int16_t((*this)[i]) );			break;
				case ElementType::INT32:	buffer = std::to_string( int32_t((*this)[i]) );			break;
				case ElementType::INT64:	buffer = std::to_string( int64_t((*this)[i]) );			break;
				case ElementType::UINT8:	buffer = std::to_string( uint8_t((*this)[i]) );			break;
				case ElementType::UINT16:	buffer = std::to_string( uint16_t((*this)[i]) );		break;
				case ElementType::UINT32:	buffer = std::to_string( uint32_t((*this)[i]) );		break;
				case ElementType::UINT64:	buffer = std::to_string( uint64_t((*this)[i]) );		break;
				case ElementType::STRING:	buffer = '\"' + (std::string)((*this)[i]) + '\"';		break;
				}
				_file.Write( buffer.c_str(), buffer.length() );
				if( i+1 < m_numElements ) _file.Write( ", ", 2 );
			}
			if( isArray ) _file.Write( "]", 1 );
		}
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::SaveAsSraw( IFile& _file ) const
	{
		// Do not save unknown garbage
		if( m_type == ElementType::UNKNOWN ) return;

		// Determine correct string type
		ElementType _storeType = m_type;
		int _stringSize;
		if( m_type == ElementType::STRING )
		{
			GetDataSize( &_stringSize );
			if( _stringSize == 2 ) _storeType = ElementType((int)_storeType+1);
			else if( _stringSize == 4 ) _storeType = ElementType((int)_storeType+2);
			else if( _stringSize == 8 ) _storeType = ElementType((int)_storeType+3);
		}

		// TYPE
		uint8_t code = (GetNumRequiredBytes(m_numElements)<<4) | uint8_t(_storeType);
		_file.Write( &code, 1 );

		// IDENTIFIER
		uint8_t length = m_name.length();
		_file.Write( &length, 1 );
		_file.Write( &m_name[0], length );

		// NELEMS
		_file.Write( &m_numElements, NELEM_SIZE(code) );

		// [SIZE]
		uint64_t dataSize = GetDataSize();
		if( m_type == ElementType::NODE || m_type == ElementType::STRING )
			_file.Write( &dataSize, 8 );

		// data
		if( m_type == ElementType::NODE )
		{
			// Recursive write.
			for( uint64_t i=0; i<m_numElements; ++i )
				((Node**)m_bufferArray)[i]->SaveAsSraw( _file );
		} else if( m_type == ElementType::STRING ) {
			for( uint64_t i=0; i<m_numElements; ++i )
			{
				uint64_t length = ((std::string*)m_bufferArray)[i].length();
				_file.Write( &length, _stringSize );
				_file.Write( ((std::string*)m_bufferArray)[i].data(), length );
			}
		} else {
			_file.Write( m_bufferArray, dataSize );
		}
	}


	// ********************************************************************* //
	const MetaFileWrapper::Node& MetaFileWrapper::Node::operator[]( const std::string& _name ) const
	{
		const Node* child;
		if( HasChild( _name, &child ) ) return *child;

		// Try to stay stable
		return UndefinedNode;
	}

	// ********************************************************************* //
	MetaFileWrapper::Node& MetaFileWrapper::Node::operator[]( const std::string& _name )
	{
		if( m_type == ElementType::UNKNOWN ) m_type = ElementType::NODE;
		if( m_type != ElementType::NODE ) throw "Node '" + m_name + "' is of an elementary type and has no named children.";
		
		// Linear search for the correct child (assumes only a few children
		// and requires array access -> no hash map)
		unsigned int m_lastAccessed = 0;
		while( m_lastAccessed < m_numElements )
		{
			if( _name == ((Node**)m_bufferArray)[m_lastAccessed]->m_name )
				return *((Node**)m_bufferArray)[m_lastAccessed];
			++m_lastAccessed;
		}

		// Not found -> create a new one (stable reaction and for write access)
		Resize(m_numElements + 1);
		((Node**)m_bufferArray)[m_lastAccessed]->SetName( _name );
		return *((Node**)m_bufferArray)[m_lastAccessed];
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::SetName( const std::string& _name )
	{
		// No additional changes required.
		// This would be the case if the parent uses some faster search structure!
		m_name = _name;
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::Resize( uint64_t _size, ElementType _type )
	{
		// Check if type is correct and set the type
		if( m_type == ElementType::UNKNOWN && _type == ElementType::UNKNOWN ) throw std::string("[Node::Reset] Current node has undefined type. Type must be defined by the Reset parameter.");
		if( m_type == ElementType::UNKNOWN )
			m_type = _type;
		if( m_type != _type && _type != ElementType::UNKNOWN ) throw std::string("[Node::Reset] Reset cannot change the type of a node.");

		m_lastAccessed = m_lastAccessed >= _size ? 0 : m_lastAccessed;

		uint64_t oldSize = ARRAY_SIZE(m_numElements, m_type);
		uint64_t newSize = ARRAY_SIZE(_size, m_type);
		// If both old and new are buffered nothing happens otherwise
		// a realloc or copy is necessary
		if( oldSize > sizeof(m_buffer) || newSize > sizeof(m_buffer) )
		{
			void* oldData = m_bufferArray;
			uint64_t minNum = min(m_numElements, _size);
			uint64_t maxNum = max(m_numElements, _size);

			// Determine target memory
			if( newSize <= sizeof(m_buffer) ) m_bufferArray = m_buffer;
			else m_bufferArray = malloc( size_t(newSize) );

			// Now (flat) copy the old data
			if( m_type == ElementType::STRING )
				for( uint64_t i=0; i<minNum; ++i )
					new ((string*)m_bufferArray + i) string(std::move(((string*)oldData)[i]));
			else
				memcpy(m_bufferArray, oldData, (size_t)min(oldSize, newSize) );

			if( oldSize > sizeof(m_buffer) ) free(oldData);
		}

		if( m_numElements < _size )
		{
			// Allocate new elements
			if( m_type == ElementType::NODE )
				for( uint64_t i=m_numElements; i<_size; ++i )
				{
					Node* newNode = (Node*)m_file->m_nodePool.Alloc();
					((Node**)m_bufferArray)[i] = new (newNode) Node( m_file, "" );
				}
			else if( m_type == ElementType::STRING )
				for( uint64_t i=m_numElements; i<_size; ++i )
					new ((string*)m_bufferArray + i) string();
		} else {
			// Correctly delete pruned elements
			if( m_type == ElementType::NODE )
				for( uint64_t i=_size; i<m_numElements; ++i )
					m_file->m_nodePool.Delete( ((Node**)m_bufferArray)[i] );
			else if( m_type == ElementType::STRING )
				for( uint64_t i=_size; i<m_numElements; ++i )
					((string*)m_bufferArray)[i].~string();
		}

		m_numElements = _size;
	}

	// ********************************************************************* //
	// Read in a single value/child node by index.
	const MetaFileWrapper::Node& MetaFileWrapper::Node::operator[]( uint64_t _index ) const
	{
		if( _index >= m_numElements ) throw "Out of bounds in node '" + m_name + "'";

		// In case of nodes there is no casting afterwards which dereferences
		// the item. The child node must be returned immediately.
		if( m_type == ElementType::NODE ) return *((Node**)m_bufferArray)[_index];

		m_lastAccessed = _index;

		// There is no new node (no memory allocations except for strings).
		return *this;
	}

	MetaFileWrapper::Node& MetaFileWrapper::Node::operator[]( uint64_t _index )
	{
		if( m_type == ElementType::UNKNOWN )
		{
			// Automatic type detection is allowed for the first element
			if( _index==0 ) return *this;
			// Otherwise this is an error
			throw std::string("[Node::operator[]] Index access to an undefined node not allowed!");
		}
		// Make array larger
		// TODO: could be faster by the use of capacity (allocate more).
		if( _index >= m_numElements ) Resize( _index+1 );

		// In case of nodes there is no casting afterwards which dereferences
		// the item. The child node must be returned immediately.
		if( m_type == ElementType::NODE ) return *((Node**)m_bufferArray)[_index];

		m_lastAccessed = _index;
		return *this;
	};

	// Casts the node data into string.
	MetaFileWrapper::Node::operator std::string() const
	{
		// Because of array access the m_buffer is the start address of
		// the string in m_bufferArray.
		return reinterpret_cast<std::string*>(m_bufferArray)[m_lastAccessed];
	}

	void* MetaFileWrapper::Node::GetData()
	{
		if( m_type == ElementType::NODE ) throw "Cannot access data from intermediate node '" + m_name + "'";
		if( m_type == ElementType::STRING ) throw "Cannot access data from string node '" + m_name + "'";

		return m_bufferArray;
	}

	// ********************************************************************* //
#define ASSIGNEMENT_OP(T, ET, TYPE_FAIL)									\
	T MetaFileWrapper::Node::operator = (T _val)							\
	{																		\
		if( m_type == ElementType::UNKNOWN ) {m_type = ET; m_numElements = 1;}				\
		if( TYPE_FAIL ) throw std::string("Cannot assign ") + #T + " to '" + m_name + "'";	\
		reinterpret_cast<T*>(m_bufferArray)[m_lastAccessed] = _val;			\
		return _val;														\
	}

	ASSIGNEMENT_OP(float, ElementType::FLOAT, ElementType::FLOAT != m_type);
	ASSIGNEMENT_OP(double, ElementType::DOUBLE, ElementType::DOUBLE != m_type);
	ASSIGNEMENT_OP(int8_t, ElementType::INT8, ElementType::INT8 != m_type);
	ASSIGNEMENT_OP(uint8_t, ElementType::UINT8, ElementType::UINT8 != m_type);
	ASSIGNEMENT_OP(int16_t, ElementType::INT16, m_type != ElementType::INT16 && m_type != ElementType::INT8);
	ASSIGNEMENT_OP(uint16_t, ElementType::UINT16, m_type != ElementType::UINT16 && m_type != ElementType::UINT8);
	ASSIGNEMENT_OP(int32_t, ElementType::INT32, m_type > ElementType::INT32 || m_type < ElementType::INT8);
	ASSIGNEMENT_OP(uint32_t, ElementType::UINT32, m_type > ElementType::UINT32 || m_type < ElementType::UINT8);
	ASSIGNEMENT_OP(int64_t, ElementType::INT64, m_type > ElementType::INT64 || m_type < ElementType::INT8);
	ASSIGNEMENT_OP(uint64_t, ElementType::UINT64, m_type > ElementType::UINT64 || m_type < ElementType::UINT8);

	bool MetaFileWrapper::Node::operator = (bool _val)
	{
		if( m_type == ElementType::UNKNOWN ) {m_type = ElementType::BIT; m_numElements=1;}
		if( ElementType::BIT != m_type ) throw std::string("Cannot assign bool to '" + m_name + "'");

		uint8_t& i = reinterpret_cast<uint8_t*>(m_bufferArray)[m_lastAccessed/8];
		uint8_t m = 1 << (m_lastAccessed & 0x7);
		i = (i & ~m) | (_val?m:0);

		return _val;
	}

	const std::string& MetaFileWrapper::Node::operator = (const std::string& _val)
	{
		if( m_type == ElementType::UNKNOWN || m_numElements==0 ) {
			m_type = ElementType::STRING;
			m_numElements = 1;
			new ((std::string*)m_bufferArray) string();
		}
		if( m_type != ElementType::STRING ) throw "Cannot assign std::string to '" + m_name + "'";

		((std::string*)m_bufferArray)[m_lastAccessed] = _val;
		return _val;
	}

	const char* MetaFileWrapper::Node::operator = (const char* _val)
	{
		if( m_type == ElementType::UNKNOWN || m_numElements==0 ) {
			m_type = ElementType::STRING;
			m_numElements = 1;
			new ((std::string*)m_bufferArray) string();
		}
		if( m_type != ElementType::STRING ) throw "Cannot assign 'const char*' to '" + m_name + "'";

		((std::string*)m_bufferArray)[m_lastAccessed] = _val;
		return _val;
	}

	// ********************************************************************* //
	// Create a sub node with an array of elementary type.
	MetaFileWrapper::Node& MetaFileWrapper::Node::Add( const std::string& _name, ElementType _type, uint64_t _numElements )
	{
		assert( _type != ElementType::UNKNOWN || _numElements == 0 );
		if( m_type != ElementType::NODE && m_type != ElementType::UNKNOWN )
			throw "It is not possible to add a child node to '" + m_name + "'. It has the wrong type.";
		// In case it was unknown set the type.
		m_type = ElementType::NODE;

		// Add a new child
		Resize(m_numElements+1);
		Node* newNode = ((Node**)m_bufferArray)[m_numElements-1];
		newNode->SetName( _name );
		if( _numElements )
			newNode->Resize( _numElements, _type );
		else {
			newNode->m_numElements = 0;
			newNode->m_type = _type;
		}

		return *newNode;
	}


	// ********************************************************************* //
	bool MetaFileWrapper::Node::HasChild( const std::string& _name, const Node** _child ) const
	{
		if( m_type == ElementType::UNKNOWN ) { if(_child) *_child = nullptr; return false;}
		if( m_type != ElementType::NODE ) throw "Node '" + m_name + "' is of an elementary type and has no named children.";

		// Linear search for the correct child (assumes only a few children
		// and requires array access -> no hash map)
		uint64_t m_lastAccessed = 0;
		while( m_lastAccessed < m_numElements )
		{
			if( _name == ((Node**)m_bufferArray)[m_lastAccessed]->m_name )
				{ if(_child) *_child = &(*((Node**)m_bufferArray)[m_lastAccessed]); return true;}
			++m_lastAccessed;
		}
		// Not found
		// Avoid that m_lastAccessed is invalid
		m_lastAccessed = m_numElements-1;
		return false;
	}

	// ********************************************************************* //
	bool MetaFileWrapper::Node::HasChild( const std::string& _name, Node** _child )
	{
		return const_cast<const Node*>(this)->HasChild(_name, (const Node**)_child);
	}


	// ********************************************************************* //
	// Recursive calculation of the size occupied in a sraw file.
	uint64_t MetaFileWrapper::Node::GetDataSize( int* _stringSize ) const
	{
		if( m_type == ElementType::NODE )
		{
			uint64_t dataSize = 0;
			for( uint64_t i=0; i<m_numElements; ++i ) {
				dataSize += ((Node**)m_bufferArray)[i]->GetDataSize();
				// Names are always stored as STRING8
				uint64_t length = ((Node**)m_bufferArray)[i]->GetName().length();
				dataSize += length + 1;
				dataSize += 1;	// CodeNType
				dataSize += uint64_t(1<<GetNumRequiredBytes(((Node**)m_bufferArray)[i]->Size()));	// NELEMS
				if(((Node**)m_bufferArray)[i]->GetType() == ElementType::NODE || ((Node**)m_bufferArray)[i]->GetType() == ElementType::STRING)
					dataSize += 8;	// Datasize
			}
			return dataSize;
		} else if( m_type == ElementType::STRING ) {
			// Go through all strings and determine the correct string type and
			// total data amount
			uint64_t lengthSum = 0, maxLength = 0;
			for( uint64_t i=0; i<m_numElements; ++i )
			{
				uint64_t length = ((std::string*)m_bufferArray)[i].length();
				maxLength = std::max( maxLength, length );
				lengthSum += length;
			}
			// Set number of required bytes to store the length if required 
			// and return the size of all length counters + string data
			int numBytes = 1<<GetNumRequiredBytes(maxLength);
			if(_stringSize) *_stringSize = numBytes;
			return m_numElements * numBytes + lengthSum;
		} else {
			return ARRAY_SIZE(m_numElements, m_type);
		}
	}

#undef ARRAY_SIZE
};
};