#
# CBFS Connect 2024 .NET Edition - Sample Project
#
# This sample project demonstrates the usage of CBFS Connect in a 
# simple, straightforward way. It is not intended to be a complete 
# application. Error handling and other checks are simplified for clarity.
#
# www.callback.com/cbfsconnect
#
# This code is subject to the terms and conditions specified in the 
# corresponding product license agreement which outlines the authorized 
# usage and restrictions.
#

ifeq ($(shell uname -s), Darwin) # MACOS
R_PATH=-Wl,-rpath,../../lib64/,-rpath,.

FRAMEWORK = -framework Carbon -framework Security
LD_FLAGS = -lcbfsconnect.24.0 -L../../lib64/ -lresolv -liconv -ldl
LD_FLAGS_SRC = -lz -lresolv -liconv -ldl
OS_CFLAGS = -D UNIX -arch arm64
MACOS = "darwin% Darwin% macos%"

all:
	g++ $(OS_CFLAGS) -D UNIX -o memdrive memdrive.cpp virtualfile.cpp  -I../../include/ $(LD_FLAGS) $(FRAMEWORK) $(R_PATH)

src: ../../src/cbfsconnect.o
	g++ $(OS_CFLAGS) -o memdrive ../../src/cbfsconnect.o memdrive.cpp virtualfile.cpp  $(LD_FLAGS_SRC) $(FRAMEWORK)

../../src/cbfsconnect.o:
	g++ -c -fno-exceptions -fno-rtti -fPIC -arch arm64 -o ../../src/cbfsconnect.o ../../src/cbfsconnect.cpp

else # LINUX
ifeq ($(shell uname -m), x86_64)
  LIB=lib64
else
  LIB=lib
endif

R_PATH=-Wl,-rpath,../../$(LIB)/,-rpath,.
LD_FLAGS = -lcbfsconnect -ldl -lpthread -L../../$(LIB)/ -ldl -lpthread
LD_FLAGS_SRC = -lz -ldl -lpthread

all:
	g++ -D UNIX -o memdrive memdrive.cpp virtualfile.cpp  -I../../include/ $(LD_FLAGS) $(R_PATH) 

src: ../../src/cbfsconnect.o
	g++ -D UNIX -o memdrive ../../src/cbfsconnect.o memdrive.cpp virtualfile.cpp  -I../../include/ $(LD_FLAGS_SRC)

../../src/cbfsconnect.o:
	g++ -c -fno-exceptions -fno-rtti -fPIC -o ../../src/cbfsconnect.o ../../src/cbfsconnect.cpp

clean:
	rm -f ../../src/*.o
	rm -f memdrive *.o
endif