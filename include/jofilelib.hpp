/**************************************************************************//**
 * \file	jofilelib.hpp
 * \brief	A file wrapper for json and structured-raw files.
 * \details This engine provides an easy interface to read and write json files
 *			and structured-raw files. The second format is an own invention.
 *			The specification can be found in \see{TODO}.
 *			The structured-raw format is convertible to json and back. It is
 *			just faster and smaller due to the fact that it is binary.
 *
 *			Include this header only and add FileSRAW[D/R/S].lib to the linker.
 * \author	Johannes Jendersie
 * \date	2013/09
 *****************************************************************************/
#pragma once

namespace Jo {
namespace Files {
	enum struct Format {
		JSON,
		SRAW
	};
};
};

#include "memfile.hpp"
#include "filewrapper.hpp"