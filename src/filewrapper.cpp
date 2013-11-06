#include "jofilelib.hpp"
#include "file.hpp"
#include "filewrapper.hpp"
#include <cctype>

namespace Jo {
namespace Files {

	const int64_t JsonSrawWrapper::ELEMENT_TYPE_SIZE[] = { -1, 1, 2, 4, 8, 1, 8, 8, 16, 16, 32, 32, 64, 64, 32, 64 };
	static int NELEM_SIZE(uint8_t _Code) { return 1<<((_Code & 0x30)>>4); }

	JsonSrawWrapper::Node JsonSrawWrapper::Node::UndefinedNode( nullptr, std::string() );

	// ********************************************************************* //
	// Use a wrapped file to read from.
	JsonSrawWrapper::JsonSrawWrapper( const IFile& _File, Format _Format ) :
		m_pNodePool(sizeof(Node)),
		RootNode(this, _File, _Format)
	{
	}

	// ********************************************************************* //
	// Create an empty wrapper for writing new files.
	JsonSrawWrapper::JsonSrawWrapper() :
		m_pNodePool(sizeof(Node)),
		RootNode(this, "Root")
	{
	}

	// ********************************************************************* //
	// Writes the wrapped data into a file.
	void JsonSrawWrapper::Write( IFile& _File, Format _Format ) const
	{
		if( _Format == Format::JSON ) 
			RootNode.SaveAsJson( _File );
		else RootNode.SaveAsSraw( _File );
	}

	// ********************************************************************* //
	void JsonSrawWrapper::Node::SaveAsJson( IFile& _File ) const
	{
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
	void JsonSrawWrapper::Node::SaveAsSraw( IFile& _File ) const
	{
		// Do not save unkown garabage
		if( m_Type == ElementType::UNKNOWN ) return;

		// TYPE
		uint8_t iCode = (GetNumRequiredBytes(m_iNumElements)<<4) | uint8_t(m_Type);
		_File.Write( &iCode, 1 );
		// TODO: determine correct string type

		// IDENTIFIER
		uint8_t iLength = m_Name.length();
		_File.Write( &iLength, 1 );
		_File.Write( &m_Name[0], iLength );

		// NELEMS
		_File.Write( &m_iNumElements, NELEM_SIZE(iCode) );

		// [SIZE]
		uint64_t iDataSize = GetDataSize();
		if( m_Type == ElementType::NODE || IsStringType( m_Type ) )
			_File.Write( &iDataSize, 8 );

		// Data
		if( m_Type == ElementType::NODE )
		{
			// Recursive write.
			for( uint64_t i=0; i<m_iNumElements; ++i )
				m_pChildren[i]->SaveAsSraw( _File );
		} else if( IsStringType(m_Type) ) {
			for( uint64_t i=0; i<m_iNumElements; ++i )
			{
				uint64_t iLength = ((std::string*)m_pBuffer)[i].length();
				_File.Write( &iLength, ELEMENT_TYPE_SIZE[(int)m_Type] );
				_File.Write( ((std::string*)m_pBuffer)[i].data(), iLength );
			}
		} else {
			if( m_iNumElements == 1 )
			{
				_File.Write( &m_iBuffer, iDataSize );
			} else {
				_File.Write( m_pBuffer, iDataSize );
			}
		}
	}





	// ********************************************************************* //
	JsonSrawWrapper::Node::Node( JsonSrawWrapper* _pWrapper, const std::string& _Name ) :
		m_pFile( _pWrapper ),
		m_iNumElements( 0 ),
		m_Type( ElementType::UNKNOWN ),
		m_pChildren( nullptr ),
		m_iBuffer( 0 ),
		m_iLastAccessed( 0 ),
		m_Name( _Name )
	{
	}

	// ********************************************************************* //
	JsonSrawWrapper::Node::Node( JsonSrawWrapper* _pWrapper, const IFile& _File, Format _Format ) :
		m_pFile( _pWrapper ),
		m_iNumElements( 0 ),
		m_Type( ElementType::UNKNOWN ),
		m_pChildren( nullptr ),
		m_iBuffer( 0 ),
		m_iLastAccessed( 0 ),
		m_Name("")
	{
		if( _Format == Format::JSON ) 
			ParseJson( _File );
		else ReadSraw( _File );
	}

	// ********************************************************************* //
	JsonSrawWrapper::Node::~Node()
	{
		if( m_pFile )	// Only if this is not a flat copy
		{
			if( m_Type == ElementType::NODE )
			{
				for( uint64_t i=0; i<m_iNumElements; ++i )
					m_pFile->m_pNodePool.Delete( m_pChildren[i] );
				free( m_pChildren );
			} else if( m_pBuffer )
			{
				if( IsStringType(m_Type) )
					delete[] (std::string*)m_pBuffer;
				else
					free( m_pBuffer );
			}
		}
	}

	// ********************************************************************* //
	// Flat copy construction. The children are just ignored.
	JsonSrawWrapper::Node::Node( const Node& _Node ) :
		m_pFile( nullptr ),		// This show the destructor that the current node is a flat copy
		m_iNumElements( _Node.m_iNumElements ),
		m_Type( _Node.m_Type ),
		m_pChildren( _Node.m_pChildren ),
		m_iBuffer( _Node.m_iBuffer ),
		m_iLastAccessed( _Node.m_iLastAccessed ),
		m_Name( _Node.m_Name )
	{
	}

	// ********************************************************************* //
	static char FindFirstNonWhitespace( const IFile& _File )
	{
		char charBuffer;
		_File.Read( 1, &charBuffer );
		while( std::isspace(charBuffer) ) _File.Read( 1, &charBuffer );
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
			// Read one character and append
			_file.Read( 1, &charBuffer );
			number += charBuffer;
			// It is a float!
			if( charBuffer == '.' || charBuffer == 'e') _isFloat = true;
		} while( charBuffer != ',' );

		number.pop_back();
		_file.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
		return number.c_str();
	}

	static JsonSrawWrapper::ElementType DeduceType( char _char )
	{
		switch(_char) {
			case '{': return JsonSrawWrapper::ElementType::NODE;
			case '"': return JsonSrawWrapper::ElementType::STRING8;
			case '[': return JsonSrawWrapper::ElementType::UNKNOWN;
			case 't': return JsonSrawWrapper::ElementType::BIT;
			case 'f': return JsonSrawWrapper::ElementType::BIT;
			case 'n': return JsonSrawWrapper::ElementType::NODE;
		}
		return JsonSrawWrapper::ElementType::UNKNOWN;
	}

	// ********************************************************************* //
	void JsonSrawWrapper::Node::ParseJson( const IFile& _File )
	{
		// First step search opening '{'
		char charBuffer = FindFirstNonWhitespace(_File);
		if( charBuffer != '{' ) throw std::string("Syntax error in json file. Expected {");
		m_Type = ElementType::NODE;

		do {
			// Now we are inside the root node search the first identifier
			charBuffer = FindFirstNonWhitespace(_File);
			if( charBuffer != '"' ) throw std::string("Syntax error in json file. Expected \"");
			std::string identifier = ReadJsonIdentifier( _File );

			// Now there must be a :
			charBuffer = FindFirstNonWhitespace(_File);
			if( charBuffer != ':' ) throw std::string("Syntax error in json file. Expected :");

			// What type has the value?
			charBuffer = FindFirstNonWhitespace(_File);
			//ElementType type =  DeduceType( charBuffer );
			Node& newNode = Add( identifier, ElementType::UNKNOWN, 0 );
			bool endArray = true;
			int index = 0;
			do {
				switch(charBuffer) {
				case '{':
					newNode.m_Type = ElementType::NODE;
					// Use recursion therfore the object must start with { -> go back
					_File.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
					newNode.ParseJson( _File );
					break;
				case '"':
					// This is a key - read name
					newNode[index] = ReadJsonIdentifier( _File );
					break;
				case '[':
					// go into the next turn
					endArray = false;
					break;
				case ',': ++index; break;
				case ']': endArray = true; break;
				case 't':
					_File.Seek( 3, IFile::SeekMode::MOVE_FORWARD );
					newNode[index] = true;
					break;
				case 'f':
					_File.Seek( 4, IFile::SeekMode::MOVE_FORWARD );
					newNode[index] = false;
					break;
				case 'n':
					// Skip null reference
					_File.Seek( 3, IFile::SeekMode::MOVE_FORWARD );
					break;
				}
				if( charBuffer >= '0' && charBuffer <= '9' )
				{
					// Parse number
					_File.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
					bool isFloat;
					std::string number = ReadJsonNumber( _File, isFloat );
					if( isFloat ) newNode[index] = atof(number.c_str());
					else newNode[index] = atoi(number.c_str());
				}
				charBuffer = FindFirstNonWhitespace(_File);
				// TODO: eof check
			} while(!endArray);
		} while(charBuffer == ',');
		if( charBuffer != '}' ) throw std::string("Syntax error in json file. Object must end with }");
	}

	// ********************************************************************* //
	// Read a string with variable sized length header from file to std::string
	static void ReadString( const IFile& _File, JsonSrawWrapper::ElementType _Type, std::string& _Out )
	{
		uint64_t iLength = 0;
		_File.Read( 1<<((int)_Type-(int)JsonSrawWrapper::ElementType::STRING8), &iLength );
		_Out.resize( size_t(iLength) );
		_File.Read( iLength, &_Out[0] );
	}

	// ********************************************************************* //
	void JsonSrawWrapper::Node::ReadSraw( const IFile& _File )
	{
		// The first byte has CODE ELEM_TYPE nibbles
		uint8_t iCodeNType;
		_File.Read( 1, &iCodeNType );
		m_Type = (ElementType)(iCodeNType & 0xf);

		// Then the identifier follows as STRING8
		ReadString( _File, ElementType::STRING8, m_Name );

		// Read NELEMS (array dimension)
		_File.Read( NELEM_SIZE(iCodeNType), &m_iNumElements );

		// Read or calculate the data block size
		uint64_t iDataSize;
		if( m_Type == ElementType::NODE || IsStringType(m_Type) )
			_File.Read( 8, &iDataSize );
		else
			iDataSize = (m_iNumElements * ELEMENT_TYPE_SIZE[(int)m_Type] + 7) / 8;

		if( m_Type == ElementType::NODE )
		{
			// Recursive read (file cursor is at the correct position).
			m_pChildren = (Node**)malloc( size_t(m_iNumElements * sizeof(Node*)) );
			for( uint64_t i=0; i<m_iNumElements; ++i )
			{
				Node* pNew = (Node*)m_pFile->m_pNodePool.Alloc();
				m_pChildren[i] = new (pNew) Node( m_pFile, _File, Format::SRAW );
			}
		} else {
			// Now the files cursor is at the beginning of the data
			if( IsStringType(m_Type) )
			{
				// Buffer single string objects
				m_pBuffer = new std::string[size_t(m_iNumElements)];
				for( uint64_t i=0; i<m_iNumElements; ++i )
					ReadString( _File, m_Type, ((std::string*)m_pBuffer)[i] );
				m_iBuffer = reinterpret_cast<uint64_t>((std::string*)m_pBuffer);
			} else if( m_iNumElements == 1 )
			{
				// Buffer small elementary type without extra memory
				_File.Read( iDataSize, &m_iBuffer );
			} else {
				// Buffer larger memory in one block.
				m_pBuffer = malloc( size_t(iDataSize) );
				_File.Read( iDataSize, m_pBuffer );
			}
		}
	}

	// ********************************************************************* //
	const JsonSrawWrapper::Node& JsonSrawWrapper::Node::operator[]( const std::string& _Name ) const
	{
		if( m_Type == ElementType::UNKNOWN ) return UndefinedNode; //throw "Invalid access to a node of unkown type: '" + m_Name + "'.";
		if( m_Type != ElementType::NODE ) throw "Node '" + m_Name + "' is of an elementary type and has no named children.";
		
		// Linear search for the correct child (assumes only a few childs
		// and requires array access -> no hash map)
		unsigned int m_iLastAccessed = 0;
		while( m_iLastAccessed < m_iNumElements )
		{
			if( _Name == m_pChildren[m_iLastAccessed]->m_Name )
				return *m_pChildren[m_iLastAccessed];
			++m_iLastAccessed;
		}

		// Try to stay stable
		return UndefinedNode;
	}

	// ********************************************************************* //
	JsonSrawWrapper::Node& JsonSrawWrapper::Node::operator[]( const std::string& _Name )
	{
		if( m_Type == ElementType::UNKNOWN ) m_Type = ElementType::NODE;
		if( m_Type != ElementType::NODE ) throw "Node '" + m_Name + "' is of an elementary type and has no named children.";
		
		// Linear search for the correct child (assumes only a few childs
		// and requires array access -> no hash map)
		unsigned int m_iLastAccessed = 0;
		while( m_iLastAccessed < m_iNumElements )
		{
			if( _Name == m_pChildren[m_iLastAccessed]->m_Name )
				return *m_pChildren[m_iLastAccessed];
			++m_iLastAccessed;
		}

		// Not found -> create a new one (stable reaktion and for write access)
		++m_iNumElements;
		m_pChildren = (Node**)realloc( m_pChildren, size_t(m_iNumElements * sizeof(Node*)) );
		Node* pNew = (Node*)m_pFile->m_pNodePool.Alloc();
		m_pChildren[m_iLastAccessed] = new (pNew) Node( m_pFile, _Name );
		return *pNew;
	}

	// ********************************************************************* //
	// Read in a single value/childnode by index.
	const JsonSrawWrapper::Node& JsonSrawWrapper::Node::operator[]( uint64_t _iIndex ) const
	{
		if( _iIndex >= m_iNumElements ) throw "Out of bounds in node '" + m_Name + "'";

		// Fast buffered element access
		if( m_iLastAccessed == _iIndex ) return *this;

		// Load the primitive data into m_iBuffer
		switch( m_Type )
		{
		case ElementType::NODE:
			m_iLastAccessed = _iIndex;
			return *m_pChildren[_iIndex];
		case ElementType::STRING8:
		case ElementType::STRING16:
		case ElementType::STRING32:
		case ElementType::STRING64:
			// Find start address in buffer and store it in m_iBuffer
			m_iBuffer = reinterpret_cast<uint64_t>(&((std::string*)m_pBuffer)[_iIndex]);
			break;
		case ElementType::BIT: m_iBuffer = (((uint32_t*)m_pBuffer)[_iIndex/32] >> (_iIndex & 0xf)) & 1; break;
		case ElementType::INT8: *(int8_t*)&m_iBuffer = ((int8_t*)m_pBuffer)[_iIndex]; break;
		case ElementType::UINT8: *(uint8_t*)&m_iBuffer = ((uint8_t*)m_pBuffer)[_iIndex]; break;
		case ElementType::INT16: *(int16_t*)&m_iBuffer = ((int16_t*)m_pBuffer)[_iIndex]; break;
		case ElementType::UINT16: *(uint16_t*)&m_iBuffer = ((uint16_t*)m_pBuffer)[_iIndex]; break;
		case ElementType::INT32: *(int32_t*)&m_iBuffer = ((int32_t*)m_pBuffer)[_iIndex]; break;
		case ElementType::UINT32: *(uint32_t*)&m_iBuffer = ((uint32_t*)m_pBuffer)[_iIndex]; break;
		case ElementType::INT64: *(int64_t*)&m_iBuffer = ((int64_t*)m_pBuffer)[_iIndex]; break;
		case ElementType::UINT64: *(uint64_t*)&m_iBuffer = ((uint64_t*)m_pBuffer)[_iIndex]; break;
		case ElementType::FLOAT: *(float*)&m_iBuffer = ((float*)m_pBuffer)[_iIndex]; break;
		case ElementType::DOUBLE: *(double*)&m_iBuffer = ((double*)m_pBuffer)[_iIndex]; break;
		}

		m_iLastAccessed = _iIndex;

		// There is no new node (no memory allocations except for strings).
		return *this;
	}

	JsonSrawWrapper::Node& JsonSrawWrapper::Node::operator[]( uint64_t _iIndex )
	{
		// Make array larger
		// TODO: could be faster by the use of capacity (allocate more).
		if( _iIndex >= m_iNumElements )
		{
			uint64_t numOld = m_iNumElements;
			uint64_t numNew = (_iIndex - m_iNumElements) + 1;
			m_iNumElements += numNew;
			switch( m_Type )
			{
			case ElementType::NODE: {
				m_pChildren = (Node**)realloc( m_pChildren, size_t(m_iNumElements * sizeof(Node*)) );
				for( uint64_t i=numOld; i<m_iNumElements; ++i )
				{
					Node* pNew = (Node*)m_pFile->m_pNodePool.Alloc();
					m_pChildren[i] = new (pNew) Node( m_pFile, "" );
				}
				} break;
			case ElementType::STRING8:
			case ElementType::STRING16:
			case ElementType::STRING32:
			case ElementType::STRING64: {
				std::string* oldBuffer = reinterpret_cast<std::string*>(m_pBuffer);
				m_pBuffer = new std::string[size_t(m_iNumElements)];
				m_iBuffer = reinterpret_cast<uint64_t>(&((std::string*)m_pBuffer)[_iIndex]);
				for( uint64_t i=0; i<numOld; ++i )
					reinterpret_cast<std::string*>(m_pBuffer)[i] = std::move(oldBuffer[i]);
				delete[] oldBuffer;
				} break;
			default: {
				uint64_t iDataSize = (m_iNumElements * ELEMENT_TYPE_SIZE[(int)m_Type] + 7) / 8;
				m_pBuffer = realloc( m_pBuffer, size_t(iDataSize) );
				} break;
			}
		}

		// Just use the constant variant and cast the result to non const.
		// This is perfectly valid because we know we are actually not constant.
		return const_cast<Node&>(const_cast<const Node&>(*this)[_iIndex]);
	};

	// Casts the node data into string.
	JsonSrawWrapper::Node::operator std::string() const
	{
		if( m_iBuffer == 0 ) return std::string("");
		// Because of array access the m_iBuffer is the start address of
		// the string in m_pBuffer.
		return *reinterpret_cast<std::string*>(m_iBuffer);
	}

	void* JsonSrawWrapper::Node::GetData()
	{
		if( m_Type == ElementType::NODE ) throw "Cannot access data from intermediate node '" + m_Name + "'";
		if( IsStringType(m_Type) ) throw "Cannot access data from string node '" + m_Name + "'";

		if( m_iNumElements == 1 )
			return &m_iBuffer;
		return m_pBuffer;
	}

	// ********************************************************************* //
#define ASSIGNEMENT_OP(T, ET)											\
	T JsonSrawWrapper::Node::operator = (T _val)						\
	{																	\
		if( m_Type == ElementType::UNKNOWN ) {m_Type = ET; m_iNumElements = 1;}				\
		if( ET != m_Type ) throw std::string("Cannot assign ") + #T + " to '" + m_Name + "'";	\
		if( m_iNumElements == 1 )										\
			*reinterpret_cast<T*>(&m_iBuffer) = _val;					\
		else reinterpret_cast<T*>(m_pBuffer)[m_iLastAccessed] = _val;	\
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

	bool JsonSrawWrapper::Node::operator = (bool _val)
	{
		if( m_Type == ElementType::UNKNOWN ) {m_Type = ElementType::BIT; m_iNumElements=1;}
		if( ElementType::BIT != m_Type ) throw std::string("Cannot assign bool to '" + m_Name + "'");

		if( m_iNumElements == 1 )
			*reinterpret_cast<bool*>(&m_iBuffer) = _val;
		else {
			uint32_t& i = reinterpret_cast<uint32_t*>(m_pBuffer)[m_iLastAccessed/32];
			uint32_t m = 1 << (m_iLastAccessed & 0xf);
			i = (i & ~m) | (_val?m:0);
		}
		return _val;
	}

	const std::string& JsonSrawWrapper::Node::operator = (const std::string& _val)
	{
		if( m_Type == ElementType::UNKNOWN ) {
			m_Type = ElementType::STRING8;
			m_iNumElements = 1;
			m_pBuffer = new std::string[1];
			m_iBuffer = reinterpret_cast<uint64_t>(m_pBuffer);
		}
		if( !IsStringType(m_Type) ) throw "Cannot assign std::string to '" + m_Name + "'";

		*reinterpret_cast<std::string*>(m_iBuffer) = _val;
		return _val;
	}

	// ********************************************************************* //
	// Create an subnode with an array of elementary type.
	JsonSrawWrapper::Node& JsonSrawWrapper::Node::Add( const std::string& _Name, ElementType _Type, uint64_t _iNumElements )
	{
		if( _iNumElements == 0 )
		{
			++m_iNumElements;
			m_pChildren = (Node**)realloc( m_pChildren, size_t(m_iNumElements * sizeof(Node*)) );
			Node* pNew = (Node*)m_pFile->m_pNodePool.Alloc();
			m_pChildren[m_iNumElements-1] = new (pNew) Node( m_pFile, _Name );
			pNew->m_Type = _Type;
			return *pNew;
		}

		assert( _Type != ElementType::NODE );
		assert( _Type != ElementType::UNKNOWN );

		// Add a new child
		++m_iNumElements;
		m_pChildren = (Node**)realloc( m_pChildren, size_t(m_iNumElements * sizeof(Node*)) );
		Node* pNew = (Node*)m_pFile->m_pNodePool.Alloc();
		m_pChildren[m_iNumElements-1] = new (pNew) Node( m_pFile, _Name );

		// Set array type and data
		pNew->m_Type = _Type;
		pNew->m_iNumElements = _iNumElements;
		if( IsStringType(_Type) )
		{
			pNew->m_pBuffer = new std::string[size_t(_iNumElements)];
			pNew->m_iBuffer = reinterpret_cast<uint64_t>(pNew->m_pBuffer);
		} else if( _iNumElements > 1 )
		{
			uint64_t iDataSize = (_iNumElements * ELEMENT_TYPE_SIZE[(int)_Type] + 7) / 8;
			pNew->m_pBuffer = malloc( size_t(iDataSize) );
		}
		return *pNew;
	}

	// ********************************************************************* //
	// Recurisve recomputation of m_iDataSize.
	uint64_t JsonSrawWrapper::Node::GetDataSize() const
	{
		if( m_Type == ElementType::NODE )
		{
			uint64_t iDataSize = 0;
			for( uint64_t i=0; i<m_iNumElements; ++i )
				iDataSize += m_pChildren[i]->GetDataSize();
			return iDataSize;
		} else if( IsStringType( ElementType::NODE ) ) {
			// Go through all strings and determine the correct string type and
			// total data amount
			uint64_t iLengthSum = 0, iMaxLength = 0;
			for( uint64_t i=0; i<m_iNumElements; ++i )
			{
				uint64_t iLength = ((std::string*)m_pBuffer)[i].length();
				iMaxLength = std::max( iMaxLength, iLength );
				iLengthSum += iLength;
			}
			// The ugly const casts bring lots of efficience - GetDataSize is the
			// only point where all strings must be iterated and during everything
			// else except saving the string type is irrelevant.
			if( iMaxLength > 0xffffffff ) { const_cast<Node*>(this)->m_Type = ElementType::STRING64; return m_iNumElements*8 + iLengthSum; }
			else if( iMaxLength > 0xffff ) { const_cast<Node*>(this)->m_Type = ElementType::STRING32; return m_iNumElements*4 + iLengthSum;	}
			else if( iMaxLength > 0xff ) { const_cast<Node*>(this)->m_Type = ElementType::STRING16; return m_iNumElements*2 + iLengthSum; }
			else { const_cast<Node*>(this)->m_Type = ElementType::STRING8; return m_iNumElements + iLengthSum; }
		} else {
			return (m_iNumElements * ELEMENT_TYPE_SIZE[(int)m_Type] + 7) / 8;
		}
	}
};
};