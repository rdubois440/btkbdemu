CFLAGS=-Wall
LIBS=-lbluetooth

all:  btkbdemu  

btkbdemu: btkbdemu.o  sdp.o
	$(CC) $(CFLAGS) $(LIBS) btkbdemu.o sdp.o -o btkbdemu -l usb

btkbdemu.o: btkbdemu.c
	gcc -c btkbdemu.c

sdp.o: sdp.c
	gcc -c sdp.c


clean:
	rm -f btkbdemu  *.o 


