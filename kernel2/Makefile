AS=x86_64-w64-mingw32-as
CC=x86_64-w64-mingw32-g++
LD=ld
OBJS = boot.o rm-main.o pm.o pm-main.o payload.o

boot: boot.ld $(OBJS)
	$(LD) -T boot.ld $(OBJS) -o $@

boot.o: boot.s
	$(AS) --32 $< -o $@

pm.o: pm.s
	$(AS) --32 $< -o $@

rm-main.s: rm-main.cpp
	$(CC) -c -std=c++20 -march=i386 -m16 -fno-ident -fno-pic -ffreestanding -fno-leading-underscore -fno-exceptions $? -masm=intel -S -o $@

rm-main.o: rm-main.s
	$(AS) --32 $< -o $@

pm-main.o: pm-main.cpp
	$(CC) -c -std=c++20 -march=i386 -m32 -fno-ident -fno-pic -ffreestanding -fno-leading-underscore -fno-exceptions -masm=intel $? -o $@

payload.exe: payload.cpp
	$(CC) $? -g -O0 -nostdlib -Wl,-e start -Wl,--image-base 0xffff800000000000 -o $@

payload.o: payload.s payload.exe
	$(AS) --32 $< -o $@

clean:
	rm *.o boot rm-main.s payload.exe
