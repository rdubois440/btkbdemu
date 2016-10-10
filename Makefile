CFLAGS=-Wall
LIBS=-lbluetooth

all:  btkbdemu  

btkbdemu: btkbdemu.o  sdp.o
	$(CC) $(CFLAGS) $(LIBS) btkbdemu.o sdp.o -o btkbdemu 

btkbdemu.o: btkbdemu.c
	$(CC) $(CFLAGS) -c btkbdemu.c

sdp.o: sdp.c
	$(CC) $(CFLAGS) -c sdp.c


clean:
	rm -f btkbdemu  *.o 


