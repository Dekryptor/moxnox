AS=x86_64-w64-mingw32-as
LD=ld

boot: boot.o boot.ld
	$(LD) -T boot.ld boot.o -o $@

boot.o: boot.s
	$(AS) $? -o $@

clean:
	rm boot.o boot
