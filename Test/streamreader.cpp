#include "../include/jofilelib.hpp"
#include <iostream>
#include <limits>
#include <Windows.h>
using namespace std;

void TestStreamReader()
{
	// Only test some float number
	assert( Jo::Files::StreamReader::ReadASCIIFloat(Jo::Files::MemFile("-1.1512",10)) == -1.1512f );
	assert( Jo::Files::StreamReader::ReadASCIIFloat(Jo::Files::MemFile("-0.00024.",10)) == -0.00024f );
	assert( Jo::Files::StreamReader::ReadASCIIFloat(Jo::Files::MemFile("0.5e-10",10)) == 0.5e-10f );
	assert( Jo::Files::StreamReader::ReadASCIIFloat(Jo::Files::MemFile("0..3f",10)) == 0.0f );
	assert( Jo::Files::StreamReader::ReadASCIIFloat(Jo::Files::MemFile("-3.141E-2",10)) == -3.141e-2f );
	assert( Jo::Files::StreamReader::ReadASCIIFloat(Jo::Files::MemFile("0",10)) == 0.0f );
	assert( Jo::Files::StreamReader::ReadASCIIFloat(Jo::Files::MemFile("1e20",10)) == 1e20f );

	// Benchmark numbers
	uint64_t start, end, cummulative=0, cummulative2=0;

	// ReadASCIIInt brute force test + benchmark
	for( int64_t i = numeric_limits<int>::min(); i<numeric_limits<int>::max(); i += 1024 )
	{
		// Convert the i into a string then try to read the string again
		char buffer[32];
		_itoa( (int)i, buffer, 10 );

		//assert( i == Jo::Files::StreamReader::ReadASCIIInt( Jo::Files::MemFile( buffer, 32 ) ) );
		QueryPerformanceCounter( (LARGE_INTEGER*)&start );
		Jo::Files::StreamReader::ReadASCIIInt( Jo::Files::MemFile( buffer, 32 ) );
		QueryPerformanceCounter( (LARGE_INTEGER*)&end );
		cummulative += end - start;

		// atoi reference benchmark
		QueryPerformanceCounter( (LARGE_INTEGER*)&start );
		atoi( buffer );
		QueryPerformanceCounter( (LARGE_INTEGER*)&end );
		cummulative2 += end - start;
	}

	std::cout << "*** Benchmark ***\n  ReadASCIIInt (ticks): " << cummulative << "\n";
	std::cout << "  atoi (ticks): " << cummulative2 << "\n";

	// ReadASCIIFloat brute benchmark
	for( int64_t i = numeric_limits<int>::min(); i<numeric_limits<int>::max(); i += 16000 )
	{
		// Convert the i into a string then try to read the string again
		string s = to_string( i / 12.0f );

		QueryPerformanceCounter( (LARGE_INTEGER*)&start );
		Jo::Files::StreamReader::ReadASCIIFloat( Jo::Files::MemFile( s.c_str(), 32 ) );
		QueryPerformanceCounter( (LARGE_INTEGER*)&end );
		cummulative += end - start;

		// atof reference benchmark
		QueryPerformanceCounter( (LARGE_INTEGER*)&start );
		atof( s.c_str() );
		QueryPerformanceCounter( (LARGE_INTEGER*)&end );
		cummulative2 += end - start;
	}

	std::cout << "  ReadASCIIFloat (ticks): " << cummulative << "\n";
	std::cout << "  atof (ticks): " << cummulative2 << "\n";
}