KERNEL_CXXFLAGS=-Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Wno-unused-const-variable -Werror=pointer-arith -fno-exceptions -fno-rtti -mno-red-zone -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -fno-threadsafe-statics -fvisibility=hidden -gdwarf-5

all: boot

boot: boot.ld boot.o boot.cpp.o payload.o
	ld -T boot.ld boot.o boot.cpp.o payload.o -o $@

boot.o: boot.S
	aarch64-unknown-linux-gnu-as boot.S -o $@

boot.cpp.o: boot.cpp
	aarch64-unknown-linux-gnu-g++ -c boot.cpp -o $@

payload.o: payload.S kernel.exe
	aarch64-unknown-linux-gnu-as payload.S -o $@

kernel.o: kernel.cpp
	clang++ -target aarch64-w64-mingw32 -c kernel.cpp ${KERNEL_CXXFLAGS} -o kernel.o

kernel.exe: kernel.o
	clang++ -target aarch64-w64-mingw32 -nostartfiles -nostdlib -Wl,--entry=startup -Wl,--image-base=0xfffff80000000000 kernel.o -o $@

clean:
	rm *.o boot kernel.exe
