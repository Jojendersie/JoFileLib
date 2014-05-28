
#include <cctype>
#include <cmath>

#include "streamreader.hpp"
#include "hybridarray.hpp"

namespace Jo {
namespace Files {

	namespace StreamReader
	{
		uint8_t SkipWhitespaces( const IFile& _file )
		{
			uint8_t charBuffer;
			do {
				if(_file.IsEof()) return 0;
				charBuffer = _file.Next();
			} while( std::isspace(charBuffer) );

			return charBuffer;
		}


		void ReadWord( const IFile& _file, std::string& _out )
		{
			_out.clear();
			try {
				uint8_t charBuffer = SkipWhitespaces(_file);
				// The first new character was already found.
				_out += charBuffer;
				// Now read until there is a new whitespace
				while( !_file.IsEof() ) {
					charBuffer = _file.Next();
					if( std::isspace(charBuffer) ) return;
					_out += charBuffer;
				}
			} catch(...) {
				// Just stop reading - eof was reached
			}
		}


		void ReadLine( const IFile& _file, std::string& _out )
		{
			_out.clear();
			try {
				// Now read until there is a 10 / '\n'
				while( !_file.IsEof() ) {
					uint8_t charBuffer = _file.Next();
					if( charBuffer == '\n' ) return;
					if( charBuffer != 13 )
						_out += charBuffer;
				}
			} catch(...) {
				// Just stop reading - eof was reached
			}
		}


		float ReadASCIIFloat( const IFile& _file )
		{
		/*	double value = 0.0;
			double div = 1.0;
			int sign = 1;
			int state = 0;	// 0-mantissa, 1-fractional part, 2-exponent

			while( true )
			{
				// Read one character and append
				uint8_t charBuffer = _file.Next();
				if( charBuffer == '-' )
				{
					if(sign == -1) break;
					sign = -1;
				} else if( charBuffer == '.' ) {
					// Starting fractional part evaluation
					if(state >= 1) break;
					state = 1;
				} else if( charBuffer == 'e' || charBuffer == 'E' ) {
					// Begin exponent evaluation
					value = sign * value / div;
					sign = 1;
					int exponent = 0;
					while( true )
					{
						// Read one character and append
						uint8_t charBuffer = _file.Next();
						if( charBuffer == '-' )
						{
							if(sign == -1) break;
							sign = -1;
						} else if( charBuffer >= '0' && charBuffer <= '9' ) {
							// No invalid case found - append
							exponent = exponent * 10 + (charBuffer - '0');
						} else break;
					}
					return float(value * pow(10.0, exponent * sign));

				} else if( charBuffer >= '0' && charBuffer <= '9' ) {
					// No invalid case found - append
					if(state == 1) div *= 10.0;
					value = value * 10.0 + (charBuffer - '0');
				} else break;
			}

			// Combine all parts to a single number
			return float(sign * value / div);	//*/


			// Read a sign and the first character to initialize value
			int value;
			int sign = 1;
			uint8_t charBuffer = SkipWhitespaces(_file);
			if( charBuffer == '-' ) {
				sign = -1;
				charBuffer = _file.Next();
			}
			if(charBuffer >= '0' && charBuffer <= '9')
				value = charBuffer - '0';
			else return 0;

			// Collect integral part
			while( true )
			{
				// Read one character and append
				uint8_t charBuffer = _file.Next();
				if( charBuffer >= '0' && charBuffer <= '9' ) {
					value = value * 10 + (charBuffer - '0');
				} else if( charBuffer == '.' ) {
					// Starting fractional part evaluation
					int fraction;
					charBuffer = _file.Next();
					if( charBuffer >= '0' && charBuffer <= '9' )
						fraction = charBuffer - '0';
					else return float(sign * value);
					double digits = 10.0;
					while( true )
					{
						charBuffer = _file.Next();
						if( charBuffer >= '0' && charBuffer <= '9' ) {
							fraction = fraction * 10 + (charBuffer - '0');
							digits *= 10.0;
						} else if( charBuffer == 'e' || charBuffer == 'E' )
						{
							// Begin exponent evaluation
							double number = sign * (value + fraction / digits);
							int exponent;
							sign = 1;
							charBuffer = _file.Next();
							if( charBuffer == '-' ) {
								sign = -1;
								charBuffer = _file.Next();
							}
							if( charBuffer >= '0' && charBuffer <= '9' )
								exponent = charBuffer - '0';
							else return float(number);
							charBuffer = _file.Next();
							// Collect remaining exponent
							while( charBuffer >= '0' && charBuffer <= '9' )
							{
								exponent = exponent * 10 + (charBuffer - '0');
								charBuffer = _file.Next();
							}
							return float(number * pow(10.0, sign * exponent));
						} else break;
					}
					return float(sign * (value + fraction / digits));
				} else if( charBuffer == 'e' || charBuffer == 'E' ) {
					// Begin exponent evaluation
					value *= sign;
					int exponent;
					sign = 1;
					charBuffer = _file.Next();
					if( charBuffer == '-' ) {
						sign = -1;
						charBuffer = _file.Next();
					}
					if( charBuffer >= '0' && charBuffer <= '9' )
						exponent = charBuffer - '0';
					else return float(value);
					charBuffer = _file.Next();
					// Collect remaining exponent
					while( charBuffer >= '0' && charBuffer <= '9' )
					{
						exponent = exponent * 10 + (charBuffer - '0');
						charBuffer = _file.Next();
					}
					return float(value * pow(10.0, sign * exponent));
				} else  break;
			}

			// No special characters (fraction or exponent) found
			return float(sign * value);	//*/

			// The number is interpreted in three parts num.numEnum.
			// The one in the middle cannot have a sign (-1 asserts this)
		/*	int sign[3] = {1,-1,1};
			int value[3] = {0,0,0};
			int numDigits[3] = {0,0,0};
			int index = 0;	// 0 (mantissa) or 1 if exponent is reached

			while( true )
			{
				// Read one character and append
				uint8_t charBuffer = _file.Next();
				if( charBuffer == '-' )
				{
					if(sign[index] == -1) break;
					sign[index] = -1;
				} else if( charBuffer == '.' ) {
					// Starting fractional part evaluation
					if(index >= 1) break;
					index = 1;
				} else if( charBuffer == 'e' || charBuffer == 'E' ) {
					// Begin exponent evaluation
					if(index == 2) break;
					index = 2;
				} else if( charBuffer >= '0' && charBuffer <= '9' ) {
					// No invalid case found - append
					value[index] = value[index] * 10 + (charBuffer - '0');
					++numDigits[index];
				} else break;
			}

			// Combine all parts to a single number
			if( numDigits[2] == 0 )
			{
				// No exponent
				if( numDigits[1] == 0 )
					return float(sign[0] * value[0]);
				else
					return float(sign[0] * (value[0] + value[1] / pow(10.0, numDigits[1])));
			} else if( numDigits[1] == 0 ) {
				// No rational part
				return float(sign[0] * value[0] * pow(10.0, value[2] * sign[2]));
			} else
				// Full number
				return float(sign[0] * (value[0] + value[1] / pow(10.0, numDigits[1])) * pow(10.0, value[2] * sign[2]));
			//*/

			// A local buffer to collect all the single characters
		/*	Jo::HybridArray<uint8_t> buffer;
			// Depending on what we have some characters could be illegal
			int state = 0;	// 1 = '-' was detected, 2 = '.', 3 = 'e'/'E', 4 = '-' for exponent
			char charBuffer;
			while( true )
			{
				// Read one character and append
				charBuffer = _file.Next();
				if( charBuffer == '-' )
				{
					if(state != 0 && state != 3) break;
					++state;
				} else if( charBuffer == '.' ) {
					if(state >= 2) break;
					state = 2;
				} else if( charBuffer == 'e' || charBuffer == 'E' ) {
					if(state >= 3) break;
					state = 3;
				} else if( charBuffer < '0' || charBuffer > '9' )
					break;
				// No invalid case found - append
				buffer.PushBack( charBuffer );
			}

			_file.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
			buffer.PushBack(0);
			return (float)atof((const char*)&buffer.First());*/
		}


		int ReadASCIIInt( const IFile& _file )
		{
			// Just failed: TODO some log-level message
			if( _file.IsEof() ) return 0;

			int sign;
			int number;

			// Check the first character - it is either a sign or a digit.
			uint8_t charBuffer = SkipWhitespaces( _file );
			if( charBuffer == '-' ) {
				// Get yet another digit to initialize number
				charBuffer = _file.Next();
				if( charBuffer <= '0' || charBuffer > '9' )
				{
					_file.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
					return 0;	// TODO: some loglevel
				}
				sign = -1;
				number = charBuffer - '0';		// number is now in [1,9]
			} else {
				if( charBuffer <= '0' || charBuffer > '9' )
				{
					_file.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
					return 0;	// TODO: some loglevel
				}
				sign = 1;
				number = charBuffer - '0';		// number is now in [1,9]
			}

			while( true )
			{
				// Read one character
				charBuffer = _file.Next();
				// Add it if it is a digit
				if( charBuffer >= '0' && charBuffer <= '9' )
					number = 10 * number + (charBuffer - '0');
				else {
					_file.Seek( 1, IFile::SeekMode::MOVE_BACKWARD );
					break;
				}
			}

			return number * sign;
		}

	} // namespace StreamReader

} // namespace Jo
} // namespace Files