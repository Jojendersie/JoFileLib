#include "../include/jofilelib.hpp"
#include <iostream>
using namespace std;

void TestPngLoad();

int main()
{
	try {
		// CREATE ********************************************
		Jo::Files::MetaFileWrapper Wrap1;
		// Write scalar stuff
		Wrap1[string("Graphic")][string("Width")] = 1024;
		Wrap1[string("Graphic")][string("Height")] = 768;
		Wrap1[string("Graphic")][string("DeviceName")] = string("Coockie");

		// Instead of repeated access cache node reference
		auto& Contr = Wrap1[string("Controlls")];
		Contr[string("Speed")] = 0.001;
		Contr[string("InvertY")] = true;

		// Create an array node
		auto& KeyMap = Contr.Add(string("Keys"), Jo::Files::MetaFileWrapper::ElementType::INT16, 100 );
		for( int i=0; i<100; ++i )
			KeyMap[i] = (int16_t)i;

		// Access an empty array and create implicit
		auto& Cookies = Contr.Add(string("Cookies"), Jo::Files::MetaFileWrapper::ElementType::INT32, 0 );
		for( int i=0; i<5; ++i )
			Cookies[i] = i;
		// Test the same for strings
		auto& Cookies2 = Contr.Add(string("Cookies2"), Jo::Files::MetaFileWrapper::ElementType::STRING8, 0 );
		Cookies2[0] = std::string("choco");
		Cookies2[1] = std::string("vanilla");


		Jo::Files::MemFile File;
		Wrap1.Write( File, Jo::Files::Format::SRAW );
		Jo::Files::HDDFile SaveTestJsonFile( "write_test.json", false );
		Wrap1.Write( SaveTestJsonFile, Jo::Files::Format::JSON );
		Jo::Files::HDDFile SaveTestSrawFile( "write_test.sraw", false );
		Wrap1.Write( SaveTestSrawFile, Jo::Files::Format::SRAW );

		// READ ************************************************
		File.Seek( 0 );
		const Jo::Files::MetaFileWrapper Wrap2( File, Jo::Files::Format::SRAW );
		auto& Gr = Wrap2[string("Graphic")];
		int w = Gr[string("Width")];
		int h = Gr[string("Height")];
		std::string DeviceName = Gr[string("DeviceName")];
		auto& Contr2 = Wrap2[string("Controlls")];
		double s = Contr2[string("Speed")];
		bool bInvert = Contr2[string("InvertY")];
		auto& KeyMap2 = Contr2[string("Keys")];
		for( int i=0; i<100; ++i )
			std::cout << (int16_t)KeyMap[i] << ' ';

		auto& RCookies = Contr2[string("Cookies")];
		std::cout << '\n';
		for( int i=0; i<5; ++i ) std::cout << (int32_t)RCookies[i] << ' ';
		auto& RCookies2 = Contr2[string("Cookies2")];
		std::cout << '\n' << (std::string)RCookies2[1] << '\n';

		// Try false access
		// Without default value this will give 0
		double d = Wrap2[string("Keks")][string("Choclate")];

		float pi = Wrap2[string("Pi")].Get(3.14159f);

		// Parse JSON ************************************************
		// TODO: benchmarl buffering for read...
		Jo::Files::HDDFile JsonFile( "example.json", true );
		const Jo::Files::MetaFileWrapper Wrap3( JsonFile, Jo::Files::Format::JSON );
		// Test output of some content
		std::cout << (std::string)Wrap3[std::string("Inhaber")][std::string("Hobbys")][2] << '\n';
		std::cout << (std::string)Wrap3[std::string("Inhaber")][std::string("Partner")];

	} catch( std::string e )
	{
		std::cout << e;
	}

	TestPngLoad();
}