#include "jofilelib.hpp"
#include "file.hpp"
#include "filewrapper.hpp"
#include <cctype>
#include <string>

namespace Jo {
namespace Files {

	const int64_t MetaFileWrapper::ELEMENT_TYPE_SIZE[] = { -1, 1, 2, 4, 8, 1, 8, 8, 16, 16, 32, 32, 64, 64, 32, 64 };
	static int NELEM_SIZE(uint8_t _code) { return 1<<((_code & 0x30)>>4); }

	MetaFileWrapper::Node MetaFileWrapper::Node::UndefinedNode( nullptr, std::string() );

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
	void MetaFileWrapper::Node::SaveAsJson( IFile& _file, int _indent ) const
	{
		// Do not save unkown garabage
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
		}

		// Add nodes recursively
		if( m_type == ElementType::NODE )
		{
			// TODO: node arrays?
			_file.Write( "{\n", 2 );
			for( uint64_t i=0; i<m_numElements; ++i )
			{
				(*this)[i].SaveAsJson( _file, _indent+2 );
				// All variables are delimeted by ,
				if(i+1<m_numElements) _file.Write( ",\n", 2 );
				else _file.Write( "\n", 1 );
			}
			for( int i=0; i<_indent; ++i ) _file.Write( " ", 1 );
			_file.Write( "}", 1 );
		} else {
			// No comes data
			// If there is more than one element add array syntax []
			int subIndent = 0;
			if( m_numElements > 1 ) { _file.Write( "[\n", 2 ); subIndent = _indent + 6; }
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
				case ElementType::UINT16:	buffer = std::to_string( uint8_t((*this)[i]) );			break;
				case ElementType::UINT32:	buffer = std::to_string( uint8_t((*this)[i]) );			break;
				case ElementType::UINT64:	buffer = std::to_string( uint8_t((*this)[i]) );			break;
				case ElementType::STRING8:
				case ElementType::STRING16:
				case ElementType::STRING32:
				case ElementType::STRING64:	buffer = '\"' + (std::string)((*this)[i]) + '\"';		break;
				}
				for( int j=0; j<subIndent; ++j ) _file.Write( " ", 1 );
				_file.Write( buffer.c_str(), buffer.length() );
				if( i+1 < m_numElements ) _file.Write( ",\n", 2 );
			}
			if( m_numElements > 1 ) _file.Write( "]", 1 );
		}

		// All variables are delimeted by ,
		//if( _indent != 0 )	// Not for root node
		//	_file.Write( "\n", 1 );
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
	void MetaFileWrapper::Node::SaveAsSraw( IFile& _file ) const
	{
		// Do not save unkown garabage
		if( m_type == ElementType::UNKNOWN ) return;

		// TYPE
		uint8_t code = (GetNumRequiredBytes(m_numElements)<<4) | uint8_t(m_type);
		_file.Write( &code, 1 );
		// TODO: determine correct string type

		// IDENTIFIER
		uint8_t length = m_name.length();
		_file.Write( &length, 1 );
		_file.Write( &m_name[0], length );

		// NELEMS
		_file.Write( &m_numElements, NELEM_SIZE(code) );

		// [SIZE]
		uint64_t dataSize = GetDataSize();
		if( m_type == ElementType::NODE || IsStringType( m_type ) )
			_file.Write( &dataSize, 8 );

		// data
		if( m_type == ElementType::NODE )
		{
			// Recursive write.
			for( uint64_t i=0; i<m_numElements; ++i )
				m_children[i]->SaveAsSraw( _file );
		} else if( IsStringType(m_type) ) {
			for( uint64_t i=0; i<m_numElements; ++i )
			{
				uint64_t length = ((std::string*)m_bufferArray)[i].length();
				_file.Write( &length, ELEMENT_TYPE_SIZE[(int)m_type] );
				_file.Write( ((std::string*)m_bufferArray)[i].data(), length );
			}
		} else {
			if( m_numElements == 1 )
			{
				_file.Write( &m_bufferArray, dataSize );
			} else {
				_file.Write( m_bufferArray, dataSize );
			}
		}
	}





	// ********************************************************************* //
	MetaFileWrapper::Node::Node( MetaFileWrapper* _wrapper, const std::string& _name ) :
		m_file( _wrapper ),
		m_numElements( 0 ),
		m_type( ElementType::UNKNOWN ),
		m_children( nullptr ),
		m_buffer( 0 ),
		m_lastAccessed( 0 ),
		m_name( _name )
	{
	}

	// ********************************************************************* //
	MetaFileWrapper::Node::Node( MetaFileWrapper* _wrapper, const IFile& _file, Format _format ) :
		m_file( _wrapper ),
		m_numElements( 0 ),
		m_type( ElementType::UNKNOWN ),
		m_children( nullptr ),
		m_buffer( 0 ),
		m_lastAccessed( 0 ),
		m_name("")
	{
		Read( _file, _format );
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::Read( const IFile& _file, Format _format )
	{
		// TODO: Format auto detection
		if( _format == Format::JSON ) 
			ParseJson( _file );
		else ReadSraw( _file );
	}

	// ********************************************************************* //
	MetaFileWrapper::Node::~Node()
	{
		if( m_file )	// Only if this is not a flat copy
		{
			if( m_type == ElementType::NODE )
			{
				for( uint64_t i=0; i<m_numElements; ++i )
					m_file->m_nodePool.Delete( m_children[i] );
				free( m_children );
			} else if( m_bufferArray )
			{
				if( IsStringType(m_type) )
					delete[] (std::string*)m_bufferArray;
				else
					free( m_bufferArray );
			}
		}
	}

	// ********************************************************************* //
	// Flat copy construction. The children are just ignored.
	MetaFileWrapper::Node::Node( const Node& _Node ) :
		m_file( nullptr ),		// This show the destructor that the current node is a flat copy
		m_numElements( _Node.m_numElements ),
		m_type( _Node.m_type ),
		m_children( _Node.m_children ),
		m_buffer( _Node.m_buffer ),
		m_lastAccessed( _Node.m_lastAccessed ),
		m_name( _Node.m_name )
	{
	}

	// ********************************************************************* //
	static char FindFirstNonWhitespace( const IFile& _file )
	{
		char charBuffer;
		if( _file.IsEof() ) throw std::string("Syntax error in json file. Unexpected end of file.");
		_file.Read( 1, &charBuffer );
		while( std::isspace(charBuffer) )
		{
			if( _file.IsEof() ) throw std::string("Syntax error in json file. Unexpected end of file.");
			_file.Read( 1, &charBuffer );
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
				_file.Read( 1, &charBuffer );
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
			_file.Read( 1, &charBuffer );
			number += charBuffer;
			// It is a float!
			if( charBuffer == '.' || charBuffer == 'e') _isFloat = true;
			// Silently accept any delimiting character.
		} while( charBuffer != ',' && charBuffer != '\n' && charBuffer != '}' );

		number.pop_back();
		_file.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
		return number.c_str();
	}

	static MetaFileWrapper::ElementType DeduceType( char _char )
	{
		switch(_char) {
			case '{': return MetaFileWrapper::ElementType::NODE;
			case '"': return MetaFileWrapper::ElementType::STRING8;
			case '[': return MetaFileWrapper::ElementType::UNKNOWN;
			case 't': return MetaFileWrapper::ElementType::BIT;
			case 'f': return MetaFileWrapper::ElementType::BIT;
			case 'n': return MetaFileWrapper::ElementType::NODE;
		}
		return MetaFileWrapper::ElementType::UNKNOWN;
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

			// What type has the value?
			charBuffer = FindFirstNonWhitespace(_file);
			//ElementType type =  DeduceType( charBuffer );
			Node& newNode = Add( identifier, ElementType::UNKNOWN, 0 );
			bool endArray = true;
			int index = 0;
			do {
				switch(charBuffer) {
				case '{':
					newNode.m_type = ElementType::NODE;
					// Use recursion therfore the object must start with { -> go back
					_file.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
					newNode.ParseJson( _file );
					break;
				case '"':
					// This is a key - read name
					newNode.m_type = ElementType::STRING8;
					newNode[index] = ReadJsonIdentifier( _file );
					break;
				case '[':
					// go into the next turn
					endArray = false;
					break;
				case ',': ++index; break;
				case ']': endArray = true; break;
				case 't':
					_file.Seek( 3, IFile::SeekMode::MOVE_FORWARD );
					newNode.m_type = ElementType::BIT;
					newNode[index] = true;
					break;
				case 'f':
					_file.Seek( 4, IFile::SeekMode::MOVE_FORWARD );
					newNode.m_type = ElementType::BIT;
					newNode[index] = false;
					break;
				case 'n':
					// Skip null reference
					_file.Seek( 3, IFile::SeekMode::MOVE_FORWARD );
					break;
				}
				if( charBuffer >= '0' && charBuffer <= '9' )
				{
					// Parse number
					_file.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
					bool isFloat;
					std::string number = ReadJsonNumber( _file, isFloat );
					if( isFloat ) {
						newNode.m_type = ElementType::DOUBLE;
						newNode[index] = atof(number.c_str());
					}
					else {
						newNode.m_type = ElementType::INT32;
						newNode[index] = atoi(number.c_str());
					}
				}
				charBuffer = FindFirstNonWhitespace(_file);
				// TODO: eof check
			} while(!endArray);
		} while(charBuffer == ',');
		if( charBuffer != '}' ) throw std::string("Syntax error in json file. Object must end with }");
	}

	// ********************************************************************* //
	// Read a string with variable sized length header from file to std::string
	static void ReadString( const IFile& _file, MetaFileWrapper::ElementType _type, std::string& _Out )
	{
		uint64_t length = 0;
		_file.Read( 1<<((int)_type-(int)MetaFileWrapper::ElementType::STRING8), &length );
		_Out.resize( size_t(length) );
		_file.Read( length, &_Out[0] );
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::ReadSraw( const IFile& _file )
	{
		// The first byte has CODE ELEM_TYPE nibbles
		uint8_t codeNType;
		_file.Read( 1, &codeNType );
		m_type = (ElementType)(codeNType & 0xf);

		// Then the identifier follows as STRING8
		ReadString( _file, ElementType::STRING8, m_name );

		// Read NELEMS (array dimension)
		_file.Read( NELEM_SIZE(codeNType), &m_numElements );

		// Read or calculate the data block size
		uint64_t dataSize;
		if( m_type == ElementType::NODE || IsStringType(m_type) )
			_file.Read( 8, &dataSize );
		else
			dataSize = (m_numElements * ELEMENT_TYPE_SIZE[(int)m_type] + 7) / 8;

		if( m_type == ElementType::NODE )
		{
			// Recursive read (file cursor is at the correct position).
			m_children = (Node**)malloc( size_t(m_numElements * sizeof(Node*)) );
			for( uint64_t i=0; i<m_numElements; ++i )
			{
				Node* newNode = (Node*)m_file->m_nodePool.Alloc();
				m_children[i] = new (newNode) Node( m_file, _file, Format::SRAW );
			}
		} else {
			// Now the files cursor is at the beginning of the data
			if( IsStringType(m_type) )
			{
				// Buffer single string objects
				m_bufferArray = new std::string[size_t(m_numElements)];
				for( uint64_t i=0; i<m_numElements; ++i )
					ReadString( _file, m_type, ((std::string*)m_bufferArray)[i] );
				m_buffer = reinterpret_cast<uint64_t>((std::string*)m_bufferArray);
			} else if( m_numElements == 1 )
			{
				// Buffer small elementary type without extra memory
				_file.Read( dataSize, &m_buffer );
			} else {
				// Buffer larger memory in one block.
				m_bufferArray = malloc( size_t(dataSize) );
				_file.Read( dataSize, m_bufferArray );
			}
		}
	}

	// ********************************************************************* //
	const MetaFileWrapper::Node& MetaFileWrapper::Node::operator[]( const std::string& _name ) const
	{
		if( m_type == ElementType::UNKNOWN ) return UndefinedNode; //throw "Invalid access to a node of unkown type: '" + m_name + "'.";
		if( m_type != ElementType::NODE ) throw "Node '" + m_name + "' is of an elementary type and has no named children.";
		
		// Linear search for the correct child (assumes only a few childs
		// and requires array access -> no hash map)
		unsigned int m_lastAccessed = 0;
		while( m_lastAccessed < m_numElements )
		{
			if( _name == m_children[m_lastAccessed]->m_name )
				return *m_children[m_lastAccessed];
			++m_lastAccessed;
		}

		// Try to stay stable
		return UndefinedNode;
	}

	// ********************************************************************* //
	MetaFileWrapper::Node& MetaFileWrapper::Node::operator[]( const std::string& _name )
	{
		if( m_type == ElementType::UNKNOWN ) m_type = ElementType::NODE;
		if( m_type != ElementType::NODE ) throw "Node '" + m_name + "' is of an elementary type and has no named children.";
		
		// Linear search for the correct child (assumes only a few childs
		// and requires array access -> no hash map)
		unsigned int m_lastAccessed = 0;
		while( m_lastAccessed < m_numElements )
		{
			if( _name == m_children[m_lastAccessed]->m_name )
				return *m_children[m_lastAccessed];
			++m_lastAccessed;
		}

		// Not found -> create a new one (stable reaktion and for write access)
		++m_numElements;
		m_children = (Node**)realloc( m_children, size_t(m_numElements * sizeof(Node*)) );
		Node* newNode = (Node*)m_file->m_nodePool.Alloc();
		m_children[m_lastAccessed] = new (newNode) Node( m_file, _name );
		return *newNode;
	}

	// ********************************************************************* //
	void MetaFileWrapper::Node::Reset( uint64_t _size, ElementType _type )
	{
		// Check if type is correct and set the type
		if( m_type == ElementType::UNKNOWN && _type == ElementType::UNKNOWN ) throw std::string("[Node::Reset] Current node has undefined type. Type must be defined by the Reset parameter.");
		else if( m_type != _type && _type != ElementType::UNKNOWN ) throw std::string("[Node::Reset] Reset cannot change the type of a node.");
		if( _type != ElementType::UNKNOWN )
			m_type = _type;

		m_lastAccessed = 0;
		switch( m_type )
		{
		case ElementType::NODE: {
			// Delete elements which are outside the range
			for( uint64_t i=_size; i<m_numElements; ++i ) m_file->m_nodePool.Delete( m_children[i] );
			// Resize
			m_children = (Node**)realloc( m_children, size_t(_size * sizeof(Node*)) );
			// If enlarged allocate more children
			for( uint64_t i=m_numElements; i<_size; ++i )
			{
				Node* newNode = (Node*)m_file->m_nodePool.Alloc();
				m_children[i] = new (newNode) Node( m_file, "" );
			}
			} break;
		case ElementType::STRING8:
		case ElementType::STRING16:
		case ElementType::STRING32:
		case ElementType::STRING64: {
			std::string* oldBuffer = reinterpret_cast<std::string*>(m_bufferArray);
			m_bufferArray = new std::string[size_t(_size)];
			m_buffer = reinterpret_cast<uint64_t>(&((std::string*)m_bufferArray)[m_lastAccessed]);
			for( uint64_t i=0; i<std::min(m_numElements,_size); ++i )
				reinterpret_cast<std::string*>(m_bufferArray)[i] = std::move(oldBuffer[i]);
			delete[] oldBuffer;
			} break;
		default: {
			uint64_t oldDataSize = (m_numElements * ELEMENT_TYPE_SIZE[(int)m_type] + 7) / 8;
			uint64_t dataSize = (_size * ELEMENT_TYPE_SIZE[(int)m_type] + 7) / 8;
			if( _size > 1 ) {
				m_bufferArray = realloc( m_bufferArray, size_t(dataSize) );
				// If there was one element before copy it to the new location
				if( m_numElements == 1 ) memcpy( m_bufferArray, &m_buffer, size_t(oldDataSize) );
			} else {
				if(m_bufferArray) memcpy( &m_buffer, m_bufferArray, size_t(dataSize) );
				free(m_bufferArray);
				m_bufferArray = nullptr;
			}
			} break;
		}

		m_numElements = _size;
	}

	// ********************************************************************* //
	// Read in a single value/childnode by index.
	const MetaFileWrapper::Node& MetaFileWrapper::Node::operator[]( uint64_t _index ) const
	{
		if( _index >= m_numElements ) throw "Out of bounds in node '" + m_name + "'";

		// In case of nodes there is no casting afterwards which dereferences
		// the item. The child node must be returned immediately.
		if( m_type == ElementType::NODE ) return *m_children[_index];

		// Fast buffered element access
		if( m_lastAccessed == _index ) return *this;

		// Load the primitive data into m_buffer
		switch( m_type )
		{
		case ElementType::STRING8:
		case ElementType::STRING16:
		case ElementType::STRING32:
		case ElementType::STRING64:
			// Find start address in buffer and store it in m_buffer
			m_buffer = reinterpret_cast<uint64_t>(&((std::string*)m_bufferArray)[_index]);
			break;
		case ElementType::BIT: m_buffer = (((uint32_t*)m_bufferArray)[_index/32] >> (_index & 0xf)) & 1; break;
		case ElementType::INT8: *(int8_t*)&m_buffer = ((int8_t*)m_bufferArray)[_index]; break;
		case ElementType::UINT8: *(uint8_t*)&m_buffer = ((uint8_t*)m_bufferArray)[_index]; break;
		case ElementType::INT16: *(int16_t*)&m_buffer = ((int16_t*)m_bufferArray)[_index]; break;
		case ElementType::UINT16: *(uint16_t*)&m_buffer = ((uint16_t*)m_bufferArray)[_index]; break;
		case ElementType::INT32: *(int32_t*)&m_buffer = ((int32_t*)m_bufferArray)[_index]; break;
		case ElementType::UINT32: *(uint32_t*)&m_buffer = ((uint32_t*)m_bufferArray)[_index]; break;
		case ElementType::INT64: *(int64_t*)&m_buffer = ((int64_t*)m_bufferArray)[_index]; break;
		case ElementType::UINT64: *(uint64_t*)&m_buffer = ((uint64_t*)m_bufferArray)[_index]; break;
		case ElementType::FLOAT: *(float*)&m_buffer = ((float*)m_bufferArray)[_index]; break;
		case ElementType::DOUBLE: *(double*)&m_buffer = ((double*)m_bufferArray)[_index]; break;
		}

		m_lastAccessed = _index;

		// There is no new node (no memory allocations except for strings).
		return *this;
	}

	MetaFileWrapper::Node& MetaFileWrapper::Node::operator[]( uint64_t _index )
	{
		if( m_type == ElementType::UNKNOWN ) throw std::string("[Node::operator[]] Index access to an undefined node not allowed!");
		// Make array larger
		// TODO: could be faster by the use of capacity (allocate more).
		if( _index >= m_numElements ) Reset( _index+1 );

		// Just use the constant variant and cast the result to non const.
		// This is perfectly valid because we know we are actually not constant.
		return const_cast<Node&>(const_cast<const Node&>(*this)[_index]);
	};

	// Casts the node data into string.
	MetaFileWrapper::Node::operator std::string() const
	{
		if( m_buffer == 0 ) return std::string("");
		// Because of array access the m_buffer is the start address of
		// the string in m_bufferArray.
		return *reinterpret_cast<std::string*>(m_buffer);
	}

	void* MetaFileWrapper::Node::GetData()
	{
		if( m_type == ElementType::NODE ) throw "Cannot access data from intermediate node '" + m_name + "'";
		if( IsStringType(m_type) ) throw "Cannot access data from string node '" + m_name + "'";

		if( m_numElements == 1 )
			return &m_buffer;
		return m_bufferArray;
	}

	// ********************************************************************* //
#define ASSIGNEMENT_OP(T, ET)											\
	T MetaFileWrapper::Node::operator = (T _val)						\
	{																	\
		if( m_type == ElementType::UNKNOWN ) {m_type = ET; m_numElements = 1;}				\
		if( ET != m_type ) throw std::string("Cannot assign ") + #T + " to '" + m_name + "'";	\
		if( m_numElements == 1 )										\
			*reinterpret_cast<T*>(&m_buffer) = _val;					\
		else reinterpret_cast<T*>(m_bufferArray)[m_lastAccessed] = _val;	\
		return _val;													\
	}

	ASSIGNEMENT_OP(float, ElementType::FLOAT);
	ASSIGNEMENT_OP(double, ElementType::DOUBLE);
	ASSIGNEMENT_OP(int8_t, ElementType::INT8);
	ASSIGNEMENT_OP(uint8_t, ElementType::UINT8);
	ASSIGNEMENT_OP(int16_t, ElementType::INT16);
	ASSIGNEMENT_OP(uint16_t, ElementType::UINT16);
	ASSIGNEMENT_OP(int32_t, ElementType::INT32);
	ASSIGNEMENT_OP(uint32_t, ElementType::UINT32);
	ASSIGNEMENT_OP(int64_t, ElementType::INT64);
	ASSIGNEMENT_OP(uint64_t, ElementType::UINT64);

	bool MetaFileWrapper::Node::operator = (bool _val)
	{
		if( m_type == ElementType::UNKNOWN ) {m_type = ElementType::BIT; m_numElements=1;}
		if( ElementType::BIT != m_type ) throw std::string("Cannot assign bool to '" + m_name + "'");

		if( m_numElements == 1 )
			*reinterpret_cast<bool*>(&m_buffer) = _val;
		else {
			uint32_t& i = reinterpret_cast<uint32_t*>(m_bufferArray)[m_lastAccessed/32];
			uint32_t m = 1 << (m_lastAccessed & 0xf);
			i = (i & ~m) | (_val?m:0);
		}
		return _val;
	}

	const std::string& MetaFileWrapper::Node::operator = (const std::string& _val)
	{
		if( m_type == ElementType::UNKNOWN ) {
			m_type = ElementType::STRING8;
			m_numElements = 1;
			m_bufferArray = new std::string[1];
			m_buffer = reinterpret_cast<uint64_t>(m_bufferArray);
		}
		if( !IsStringType(m_type) ) throw "Cannot assign std::string to '" + m_name + "'";

		*reinterpret_cast<std::string*>(m_buffer) = _val;
		return _val;
	}

	// ********************************************************************* //
	// Create an subnode with an array of elementary type.
	MetaFileWrapper::Node& MetaFileWrapper::Node::Add( const std::string& _name, ElementType _type, uint64_t _numElements )
	{
		if( _numElements == 0 )
		{
			++m_numElements;
			m_children = (Node**)realloc( m_children, size_t(m_numElements * sizeof(Node*)) );
			Node* newNode = (Node*)m_file->m_nodePool.Alloc();
			m_children[m_numElements-1] = new (newNode) Node( m_file, _name );
			newNode->m_type = _type;
			return *newNode;
		}

		assert( _type != ElementType::NODE );
		assert( _type != ElementType::UNKNOWN );

		// Add a new child
		++m_numElements;
		m_children = (Node**)realloc( m_children, size_t(m_numElements * sizeof(Node*)) );
		Node* newNode = (Node*)m_file->m_nodePool.Alloc();
		m_children[m_numElements-1] = new (newNode) Node( m_file, _name );

		// Set array type and data
		newNode->m_type = _type;
		newNode->m_numElements = _numElements;
		if( IsStringType(_type) )
		{
			newNode->m_bufferArray = new std::string[size_t(_numElements)];
			newNode->m_buffer = reinterpret_cast<uint64_t>(newNode->m_bufferArray);
		} else if( _numElements > 1 )
		{
			uint64_t dataSize = (_numElements * ELEMENT_TYPE_SIZE[(int)_type] + 7) / 8;
			newNode->m_bufferArray = malloc( size_t(dataSize) );
		}
		return *newNode;
	}

	// ********************************************************************* //
	// Recurisve recomputation of m_iDataSize.
	uint64_t MetaFileWrapper::Node::GetDataSize() const
	{
		if( m_type == ElementType::NODE )
		{
			uint64_t dataSize = 0;
			for( uint64_t i=0; i<m_numElements; ++i )
				dataSize += m_children[i]->GetDataSize();
			return dataSize;
		} else if( IsStringType( ElementType::NODE ) ) {
			// Go through all strings and determine the correct string type and
			// total data amount
			uint64_t lengthSum = 0, maxLength = 0;
			for( uint64_t i=0; i<m_numElements; ++i )
			{
				uint64_t length = ((std::string*)m_bufferArray)[i].length();
				maxLength = std::max( maxLength, length );
				lengthSum += length;
			}
			// The ugly const casts bring lots of efficience - GetDataSize is the
			// only point where all strings must be iterated and during everything
			// else except saving the string type is irrelevant.
			if( maxLength > 0xffffffff ) { const_cast<Node*>(this)->m_type = ElementType::STRING64; return m_numElements*8 + lengthSum; }
			else if( maxLength > 0xffff ) { const_cast<Node*>(this)->m_type = ElementType::STRING32; return m_numElements*4 + lengthSum;	}
			else if( maxLength > 0xff ) { const_cast<Node*>(this)->m_type = ElementType::STRING16; return m_numElements*2 + lengthSum; }
			else { const_cast<Node*>(this)->m_type = ElementType::STRING8; return m_numElements + lengthSum; }
		} else {
			return (m_numElements * ELEMENT_TYPE_SIZE[(int)m_type] + 7) / 8;
		}
	}
};
};