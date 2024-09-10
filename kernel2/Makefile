AS=x86_64-w64-mingw32-as
CC=x86_64-w64-mingw32-g++
LD=ld

boot: boot.o boot.ld main.o pm.o pm-main.o
	$(LD) -T boot.ld boot.o main.o pm.o pm-main.o -o $@

boot.o: boot.s
	$(AS) --32 $? -o $@

pm.o: pm.s
	$(AS) --32 $? -o $@

main.s: main.cpp
	$(CC) -c -std=c++20 -march=i386 -m16 -fno-ident -fno-pic -ffreestanding -fno-leading-underscore -fno-exceptions $? -masm=intel -S -o $@

main.o: main.s
	$(AS) --32 $? -o $@

pm-main.o: pm-main.cpp
	$(CC) -c -std=c++20 -march=i386 -m32 -fno-ident -fno-pic -ffreestanding -fno-leading-underscore -fno-exceptions $? -o $@

clean:
	rm *.o boot main.s
