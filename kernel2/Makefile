AS=x86_64-w64-mingw32-as
CC=x86_64-w64-mingw32-g++
LD=ld

boot: boot.o boot.ld main.o
	$(LD) -T boot.ld boot.o main.o -o $@

boot.o: boot.s
	$(AS) $? -o $@

main.o: main.cpp
	$(CC) -c -march=i386 -m16 -fno-ident -fno-pic -ffreestanding -fno-leading-underscore -fno-exceptions $? -o $@

clean:
	rm *.o boot
