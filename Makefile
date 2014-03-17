include config.mk
-include $(BUILDDIR)*.dep

default: library

library: jofilelib_a

INC = -I$(CURDIR)/include/ -I$(CURDIR)/../JoMemory/include/
BUILDDIR = $(CURDIR)/make/

# NOTE: GNU Make defines the rule below implicitly.
#	   -- It would be nicer to put the dependency options here! --
$(BUILDDIR)%.o: src/%.cpp
	- $(CXX) $(INC) -c $(CF) $(CXXFLAGS) $< -o $@

# NOTE: we are listing only the objects here that do not make
#		become executables (as, e.g., test_jofile.o)
OBJ = $(BUILDDIR)fileutils.o $(BUILDDIR)fileutils_unix.o $(BUILDDIR)fileutils_win.o $(BUILDDIR)filewrapper.o $(BUILDDIR)hddfile.o $(BUILDDIR)imagewrapper.o $(BUILDDIR)imagewrapper_pfm.o $(BUILDDIR)imagewrapper_png.o $(BUILDDIR)memfile.o $(BUILDDIR)streamreader.o

LIB = -lrt

# MinGW, Cygwin, VC++
#
# LIB =

jofilelib_a: $(OBJ)
	- ar cru jofilelib.a $(OBJ)
	- ranlib jofilelib.a
	
jofilelib_so: $(OBJ)
	- $(CXX) -shared -o $@ $^
	
test_jofile: $(OBJ) test_jofile.o
	- $(CXX) $(CF) $(CXXFLAGS) -o test_jofile test_jofile.o $(OBJ) $(LIB) $(INC)
  
# run "test"
test: test_jofile 
	- ./test_jofile
 
clobber:
	- rm *.o *.dep

clean:
	- rm *.o *.dep test_jofile
# - rm *.a *.so 