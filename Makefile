include config.mk
-include *.dep

default: library

library: jofile.a

# NOTE: GNU Make defines the rule below implicitly.
#	   -- It would be nicer to put the dependency options here! --
%.o: src/%.cpp
	- $(CXX) -c $(CF) $(CXXFLAGS) $< -o $@

# NOTE: we are listing only the objects here that do not make
#		become executables (as, e.g., test_jofile.o)
OBJ = fileutils.o fileutils_unix.o fileutils_win.o filewrapper.o hddfile.o imagewrapper.o imagewrapper_pfm.o imagewrapper_png.o memfile.o streamreader.o

LIB = -lrt

# MinGW, Cygwin, VC++
#
# LIB =

jofile.a: $(OBJ)
	- $(ARCHIVE) jofile.a $(OBJ)
	- $(RANLIB) jofile.a
	
jofile.so: $(OBJ)
	- $(CXX) -shared -o $@ $^
	
test_jofile: $(OBJ) test_jofile.o
	- $(CXX) $(CF) $(CXXFLAGS) -o test_jofile test_jofile.o $(OBJ) $(LIB)
  
# run "test"
test: test_jofile 
	- ./test_jofile
 
clobber:
	- rm *.o *.dep

clean:
	- rm *.o *.dep test_jofile
# - rm *.a *.so 