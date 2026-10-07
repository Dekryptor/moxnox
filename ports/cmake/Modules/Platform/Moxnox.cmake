# Moxnox: PE/COFF binaries with a POSIX-flavoured userland.
# Artifact naming follows the mingw conventions (lib*.dll next to a
# lib*.dll.a import library, .exe executables); everything else
# behaves like a unix, so if(UNIX) code paths apply and if(WIN32)
# ones do not.  Modeled on Platform/CYGWIN.cmake, which makes the
# same split.
set(MOXNOX 1)

set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".dll")
set(CMAKE_SHARED_MODULE_PREFIX "lib")
set(CMAKE_SHARED_MODULE_SUFFIX ".dll")
set(CMAKE_IMPORT_LIBRARY_PREFIX "lib")
set(CMAKE_IMPORT_LIBRARY_SUFFIX ".dll.a")
set(CMAKE_EXECUTABLE_SUFFIX ".exe")
# DLLs are installed in /lib with the other libraries, not in /bin
# next to the executables; install(TARGETS) routes them through the
# LIBRARY properties rather than the RUNTIME ones.
set(CMAKE_DLL_INSTALL_AS_LIBRARY 1)
# Modules have a different default prefix than shared libs.
set(CMAKE_MODULE_EXISTS 1)

# dlopen and friends live in mnposix.dll, which the compiler's
# default libraries already pull in.
set(CMAKE_DL_LIBS "")

set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a" ".a")

include(Platform/UnixPaths)
