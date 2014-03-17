CXX 	 = g++

CXXFLAGS =  -Wp,-MMD,$*.dep \
            -Wall -Wextra -pedantic \
            -fPIC \
            -g

# MinGW

#CXXFLAGS =  -D__NO_RAND48 -D__VC_DONT_USE_RT_CLOCK \
#            -Wp,-MMD,$*.dep \
#            -Wall -Wextra -pedantic \
#            -g

# Cygwin
#
#CXXFLAGS =  -Wp,-MMD,$*.dep \
#            -Wall -Wextra -pedantic \
#            -D__VC_DONT_USE_RT_CLOCK \
#            -g

#LDFLAGS = -g -- we don't care for the linker for now

# NOTES
# =====
#
# The variable CXX (CC) specifies the C++ (C) compiler.
#
# CXXFLAGS (CFLAGS) specify compiler flags (command line options).
#  "-Wl,..." and "-Wp,..." are passed to the linker and preprocessor,
#  respectively. For convencience, we let CXX do the linking, i.e.,
#  the linker "ld" with C/C++ standard libraries.
#
# LDFLAGS specifies linker flags (as provided to CXX, see above).
#
# Compiler flags (on Unix see "man gcc" and "man cpp")
#
# -Wp,-MMD,$*.dep : let compiler (more precise preprocessor "cpp")
#		    generate dependencies automatically
#		    (see *.dep files)
#                   "-MMD" vs. "-MD" =
#			do not vs do depend on system header files
#                   (on Unix, see "man cpp")
# -Wall -pedantic : turn on all warnings
# -fPIC :           operating system related, see man pages
#                   (usually required for 64bit targets)
# -g :              include debug information
#
# -O2 -DNDEBUG :    turn on optimization, turn assertions off
#                   aka "release mode"
#   Notes:
#   + Don't mix "-g" and "-O" (or stick to "-O0"), the optimizer
#     confuses the debugger (e.g., with variables kept in registers,
#     lacking stack frames, etc.) !
#     => Use either one or the other option !
#
#   + You can have more optimization, try "-O6 -mtune=native"
#
#     gcc -c -Q -O3 --help=optimizers
#
#      lists the particular optimization passes enabled for -O3
#      (replace by -O3,...,-O6), see gcc manual
#
#     gcc -march=native -E -v - </dev/null 2>&1 | sed -n 's/.* -v - //p'
#
#      shows the options equivalent to "-mtune=native" on *your* machine,
#      note that this may degrade (or not even run) on machines other than
#      yours!
#
#     On 32bit systems w/o "-mtune=native" you may have to enable
#     advanced floating point computations like manually like,
#     e.g. "-msse".
#
# - "-lrt" linkes the real time clock library (Linux)
