CXXFLAGS=-Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Wno-unused-const-variable -Werror=pointer-arith -fno-exceptions -fno-rtti -fcheck-new -Wno-invalid-offsetof -fno-threadsafe-statics -fvisibility=hidden -fno-auto-import
BOOT_CXXFLAGS=${CXXFLAGS} -fno-asynchronous-unwind-tables
KERNEL_CXXFLAGS=${CXXFLAGS} -mno-red-zone -mno-stack-arg-probe -gdwarf-5

all: boot

boot: boot.ld boot.o boot.cpp.o payload.o
	ld -T boot.ld boot.o boot.cpp.o payload.o -o $@

boot.o: boot.S
	aarch64-w64-mingw32-as boot.S -o $@

boot.cpp.o: boot.cpp
	clang++ -target aarch64-w64-mingw32 -stdlib=libc++ -std=c++23 -mstrict-align -gdwarf-5 -I../include -c boot.cpp ${BOOT_CXXFLAGS} -o $@

payload.o: payload.S kernel.exe
	aarch64-w64-mingw32-as payload.S -o $@

kernel.o: kernel.cpp
	clang++ -target aarch64-w64-mingw32 -stdlib=libc++ -c kernel.cpp ${KERNEL_CXXFLAGS} -o kernel.o

kernel.exe: kernel.o
	clang++ -target aarch64-w64-mingw32 -nostartfiles -nostdlib -Wl,--entry=startup -Wl,--image-base=0xfffff80000000000 kernel.o -o $@

clean:
	rm *.o boot kernel.exe
