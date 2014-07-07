#include "../include/jofilelib.hpp"
#include <iostream>
using namespace std;

void TestPngLoad()
{

	try {
		std::cout << "\n******* png loader test *******\n";
		// Load a png file without interlacing
		Jo::Files::HDDFile file("normal.png");
		Jo::Files::ImageWrapper normalPng( file, Jo::Files::Format::PNG );

		std::cout << "Loaded normal.png:\n";
		std::cout << "\tWidth x Height: " << normalPng.Width() << " x " << normalPng.Height() << '\n';
		std::cout << "\tChannels: " << normalPng.NumChannels() << '\n';
		std::cout << "\tBitdepth: " << normalPng.BitDepth() << '\n';

		Jo::Files::HDDFile file2("interlaced.png");
		Jo::Files::ImageWrapper interlacedPng( file2, Jo::Files::Format::PNG );

		std::cout << "Loaded interlaced.png:\n";
		std::cout << "\tWidth x Height: " << interlacedPng.Width() << " x " << interlacedPng.Height() << '\n';
		std::cout << "\tChannels: " << interlacedPng.NumChannels() << '\n';
		std::cout << "\tBitdepth: " << interlacedPng.BitDepth() << '\n';

		// Both images should contain the same stuff!
		if( 0== memcmp( normalPng.GetBuffer(), interlacedPng.GetBuffer(),
			normalPng.BitDepth() * normalPng.Width() * normalPng.Height() / 8 ) )
			std::cout << "Contents of both are equal.\n";
		else std::cout << "Error images are different!\n";

		// Make opaque
		for( unsigned y=0; y<normalPng.Height(); ++y )
			for( unsigned x=0; x<normalPng.Width(); ++x )
			{
				normalPng.Set(x,y,3, 1.0f);
			}

		Jo::Files::HDDFile file3("out.png", Jo::Files::HDDFile::OVERWRITE);
		normalPng.Write( file3, Jo::Files::Format::PNG );

		std::cout << "\n******* tga loader test *******\n";
		Jo::Files::ImageWrapper rleTga( Jo::Files::HDDFile("rle.tga"), Jo::Files::Format::TGA );
		rleTga.Write( Jo::Files::HDDFile("out.tga", Jo::Files::HDDFile::OVERWRITE), Jo::Files::Format::TGA );

		int breakpoint;
	} catch( std::string e )
	{
		std::cout << e;
	}
}