CFLAGS=-Wall
LIBS=-lbluetooth

all:  btkbdemu  

btkbdemu: btkbdemu.o 
	$(CC) $(CFLAGS) $(LIBS) btkbdemu.o  -o btkbdemu 

clean:
	rm -f btkbdemu  *.o 


