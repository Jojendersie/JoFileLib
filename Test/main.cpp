#include "../include/jofilelib.hpp"
#include <iostream>

// CRT's memory leak detection
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
// The report also states the allocation number in {}.
// To find a memory leak use _CrtSetBreakAlloc(##); at program start.
// You could also use _CrtDumpMemoryLeaks(); at any location to look
// when a memory leak appears.
#endif

#include <Windows.h>

using namespace std;

void TestPngLoad();
void TestRndAccessHDDFile();
void TestStreamReader();
void TestUtilities();

int main()
{
#if defined(DEBUG) || defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	//_CrtSetBreakAlloc( 449 );
#endif

	TestUtilities();
	//TestStreamReader();

	// Benchmark of single byte read
	/*{
		Jo::Files::HDDFile file("D:/Studium/WiSe1213/Arbeit-rohdaten/data/HR_uv75_0030-5thday.nc");
		char buf;
		uint64_t start, end;
		QueryPerformanceCounter( (LARGE_INTEGER*)&start );
		while(!file.IsEof())
		{
			//file.Read(1, &buf);	// 19895168
			buf = file.Next();		// 13073322
		}
		QueryPerformanceCounter( (LARGE_INTEGER*)&end );
		std::cout << (end-start);
	}//*/
	
	try {
		// CREATE ********************************************
		Jo::Files::MetaFileWrapper Wrap1;
		// Write scalar stuff
		Wrap1[string("Graphic")][string("Width")] = 1024;
		Wrap1[string("Graphic")][string("Height")] = 768;
		Wrap1[string("Graphic")][string("DeviceName")] = string("Cookie");

		// Instead of repeated access cache node reference
		auto& Contr = Wrap1[string("Controls")];
		Contr[string("Speed")] = 0.001;
		Contr[string("InvertY")] = true;

		// Create an array node
		auto& KeyMap = Contr.Add(string("Keys"), Jo::Files::MetaFileWrapper::ElementType::INT16, 10 );
		for( int i=0; i<10; ++i )
			KeyMap[i] = (int16_t)i;

		// Access an empty array and create implicit
		auto& Cookies = Contr.Add(string("Cookies"), Jo::Files::MetaFileWrapper::ElementType::INT32, 0 );
		for( int i=0; i<5; ++i )
			Cookies[i] = i;
		// Test the same for strings
		auto& Cookies2 = Contr.Add(string("Cookies2"), Jo::Files::MetaFileWrapper::ElementType::STRING, 0 );
		Cookies2[0] = std::string("choco");
		Cookies2[1] = std::string("vanilla");

		// Create an array of arrays with only few elements
		auto& AA = Contr.Add(string("AAtest"), Jo::Files::MetaFileWrapper::ElementType::NODE, 2);
		AA[0][0] = 42;
		AA[1][0] = 24;
		AA[1][1] = 96;


		Jo::Files::MemFile File;
		Wrap1.Write( File, Jo::Files::Format::SRAW );
		Jo::Files::HDDFile SaveTestJsonFile( "write_test.json", Jo::Files::HDDFile::CREATE_FILE );
		Wrap1.Write( SaveTestJsonFile, Jo::Files::Format::JSON );
		Jo::Files::HDDFile SaveTestSrawFile( "write_test.sraw", Jo::Files::HDDFile::CREATE_FILE );
		Wrap1.Write( SaveTestSrawFile, Jo::Files::Format::SRAW );

		// READ ************************************************
		File.Seek( 0 );
		const Jo::Files::MetaFileWrapper Wrap2( File );
		auto& Gr = Wrap2[string("Graphic")];
		int w = Gr[string("Width")];
		int h = Gr[string("Height")];
		assert( w==1024 && h==768 );
		// Test silent casts
		int8_t w8 = Gr[string("Width")];
		int16_t w16 = Gr[string("Width")];
		int64_t w64 = Gr[string("Width")];

		std::string DeviceName = Gr[string("DeviceName")];
		auto& Contr2 = Wrap2[string("Controls")];
		double s = Contr2[string("Speed")];
		bool bInvert = Contr2[string("InvertY")];
		auto& KeyMap2 = Contr2[string("Keys")];
		for( int i=0; i<10; ++i )
			std::cout << (int16_t)KeyMap2[i] << ' ';

		auto& RCookies = Contr2[string("Cookies")];
		std::cout << '\n';
		for( int i=0; i<5; ++i ) std::cout << (int32_t)RCookies[i] << ' ';
		auto& RCookies2 = Contr2[string("Cookies2")];
		std::cout << '\n' << (std::string)RCookies2[1] << '\n';

		// Try false access
		// Without default value this will give 0
		double d = Wrap2[string("Keks")][string("Chocolate")];

		float pi = Wrap2[string("Pi")].Get(3.14159f);

		// Parse JSON ************************************************
		// TODO: benchmark buffering for read...
		Jo::Files::HDDFile JsonFile( "example.json" );
		const Jo::Files::MetaFileWrapper Wrap3( JsonFile );
		// Test output of the content
		std::cout << (string)Wrap3[string("StringProperty")] << '\n';
		std::cout << (int)Wrap3[string("Number")] << '\n';
		std::cout << (double)Wrap3[string("Object")][string("ArrayArray")][1][0] << '\n';
		std::cout << (string)Wrap3[string("ObjectArray")][0][string("Name")] << '\n';
		std::cout << (bool)Wrap3[string("ObjectArray")][1][string("Extra")] << '\n';

	} catch( std::string e )
	{
		std::cout << e;
	}

	TestRndAccessHDDFile();
	TestPngLoad();
}



void TestRndAccessHDDFile()
{
	Jo::Files::HDDFile file( "rndaccess.test", Jo::Files::HDDFile::CREATE_FILE );

	// Sequential write
	for( int i=0; i<10; ++i )
		file.Write( &i, sizeof(int) );

	// Skip back (something like "correct header")
	file.Seek( 0 );
	int buf = 1000;
	file.Write( &buf, sizeof(int) );

	// Skip into any area
	file.Seek( sizeof(int) * 15 );
	file.Write( &buf, sizeof(int) );

	// Go back and test data
	file.Seek( 0 );
	file.Read( sizeof(int), &buf );
	if( buf != 1000 ) std::cout << "[TestRndAccessHDDFile] Reading first int failed.\n";
	for( int i=1; i<10; ++i )
	{
		file.Read( sizeof(int), &buf );
		if( buf != i ) std::cout << "[TestRndAccessHDDFile] Reading list of ints failed.\n";
	}
	// Output the undefined part + the last 1000
	for( int i=11; i<16; ++i )
	{
		file.Read( sizeof(int), &buf );
		std::cout << buf << ", ";
	}
	file.Read( sizeof(int), &buf );
	if( buf == 1000 ) std::cout << "\nRandom access test OK\n";
	else std::cout << "\nRandom access test failed\n";
}