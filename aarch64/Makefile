all: boot

boot: boot.ld boot.o boot.cpp.o
	ld -T boot.ld boot.o boot.cpp.o -o $@

boot.o: boot.S
	aarch64-unknown-linux-gnu-as boot.S -o $@

boot.cpp.o: boot.cpp
	aarch64-unknown-linux-gnu-g++ -c boot.cpp -o $@

clean:
	rm *.o boot
