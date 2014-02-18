#include "../include/jofilelib.hpp"
#include <iostream>
#include <cassert>
using namespace std;
using namespace Jo::Files::Utils;

void TestUtilities()
{
	// Test if IsEqual is robust and correct
	assert( !IsEqual( "", "" ) );
	assert( !IsEqual( "bla", "bla" ) );
	assert( IsEqual( "utils.cpp", "utils.cpp" ) );
	assert( IsEqual( "utils.cpp", "../Test/utils.cpp" ) );
	assert( IsEqual( "debug", "Debug" ) );
}