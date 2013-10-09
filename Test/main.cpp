#include "../include/jofilelib.hpp"
#include <iostream>
using namespace std;

int main()
{
	try {
		// CREATE ********************************************
		Jo::Files::JsonSrawWrapper Wrap1;
		// Write scalar stuff
		Wrap1.RootNode[string("Graphic")][string("Width")] = 1024;
		Wrap1.RootNode[string("Graphic")][string("Height")] = 768;
		Wrap1.RootNode[string("Graphic")][string("DeviceName")] = string("Coockie");

		// Instead of repeated access cache node reference
		auto& Contr = Wrap1.RootNode[string("Controlls")];
		Contr[string("Speed")] = 0.001;
		Contr[string("InvertY")] = true;

		// Create an array node
		auto& KeyMap = Contr.Add(string("Keys"), Jo::Files::JsonSrawWrapper::ElementType::INT16, 100 );
		for( int i=0; i<100; ++i )
			KeyMap[i] = (int16_t)i;

		Jo::Files::MemFile File;
		Wrap1.Write( File, Jo::Files::Format::SRAW );

		// READ ************************************************
		File.Seek( 0 );
		const Jo::Files::JsonSrawWrapper Wrap2( File, Jo::Files::Format::SRAW );
		auto& Gr = Wrap2.RootNode[string("Graphic")];
		int w = Gr[string("Width")];
		int h = Gr[string("Height")];
		std::string DeviceName = Gr[string("DeviceName")];
		auto& Contr2 = Wrap2.RootNode[string("Controlls")];
		double s = Contr2[string("Speed")];
		bool bInvert = Contr2[string("InvertY")];
		auto& KeyMap2 = Contr2[string("Keys")];
		for( int i=0; i<100; ++i )
			std::cout << (int16_t)KeyMap[i] << ' ';

		// Try false access
		// Without default value this will give 0
		double d = Wrap2.RootNode[string("Keks")][string("Choclate")];

		float pi = Wrap2.RootNode[string("Pi")].Get(3.14159f);

	} catch( std::string e )
	{
		std::cout << e;
	}
}