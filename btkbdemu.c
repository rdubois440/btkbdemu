// Todo
//

// Notes
// input character is read from stdin in non canonical mode. See set_conio_terminal_mode
// input can be 1 or more bytes
// output is the usb standard defined in USB - HID Usage Tables




#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/input.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sdp.h>
#include <bluetooth/hidp.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

#include "hid.h"
#include "sdp.h"


#include <fcntl.h>
//#include <cstdlib>
#include <stdio.h>
#include <string.h>

struct termios orig_termios;

#define DEBUG_LEVEL 0

#define PROTOCOL_MOUSE 2
#define PROTOCOL_KEYBOARD 1

//using namespace std;

struct usb_bus *busses,*bus,*dbus;
char translateKey(char);
char getRepeatedKey(char);

int cs;
int is;



void reset_terminal_mode()
{
	tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
	struct termios new_termios;

	/* take two copies - one for now, one for later */
	tcgetattr(0, &orig_termios);
	memcpy(&new_termios, &orig_termios, sizeof(new_termios));

	/* register cleanup handler, and set the new terminal mode */
	atexit(reset_terminal_mode);
	cfmakeraw(&new_termios);
	tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
	struct timeval tv = { 0L, 10000L };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
	int r;
	unsigned char c;
	if ((r = read(0, &c, sizeof(c))) < 0) {
		{
			//printf("Error - getch returns %x \r\n", c);	
			return r;
		}
	} else {
		//printf("getch returns %02x", c);	
		return c;
	}
}

/*
 *  taken from BlueZ
 */
static int l2cap_listen(const bdaddr_t *bdaddr, unsigned short psm, int lm, int backlog)
{
	struct sockaddr_l2 addr;
	struct l2cap_options opts;
	int sk;

	perror("before socket:");
	if ((sk = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0)
	{
		perror("after socket error:");
		return -1;
	}
	perror("after socket success:");
	fprintf(stderr, "socket returns %d\n", sk);
	memset(&addr, 0, sizeof(addr));
	addr.l2_family = AF_BLUETOOTH;
	bacpy(&addr.l2_bdaddr, bdaddr);
	addr.l2_psm = htobs(psm);
	//fprintf(stderr, "before bind %s\n", bdaddr);

	if (bind(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("after bind error:");
		close(sk);
		return -1;
	}
	perror("after bind success:");

	setsockopt(sk, SOL_L2CAP, L2CAP_LM, &lm, sizeof(lm));

	memset(&opts, 0, sizeof(opts));
	opts.imtu = HIDP_DEFAULT_MTU;
	opts.omtu = HIDP_DEFAULT_MTU;
	opts.flush_to = 0xffff;

	setsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS, &opts, sizeof(opts));

	if (listen(sk, backlog) < 0) {
		close(sk);
		return -1;
	}
	perror("after listen:");

	return sk;
}

/*
 *  taken from BlueZ
 */
static int l2cap_connect(bdaddr_t *src, bdaddr_t *dst, unsigned short psm)
{
	struct sockaddr_l2 addr;
	struct l2cap_options opts;
	int sk;

	if ((sk = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.l2_family  = AF_BLUETOOTH;
	bacpy(&addr.l2_bdaddr, src);

	if (bind(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		close(sk);
		return -1;
	}
	perror("after bind:");

	memset(&opts, 0, sizeof(opts));
	opts.imtu = HIDP_DEFAULT_MTU;
	opts.omtu = HIDP_DEFAULT_MTU;
	opts.flush_to = 0xffff;

	setsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS, &opts, sizeof(opts));

	memset(&addr, 0, sizeof(addr));
	addr.l2_family  = AF_BLUETOOTH;
	bacpy(&addr.l2_bdaddr, dst);
	addr.l2_psm = htobs(psm);

	if (connect(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		close(sk);
		return -1;
	}
	perror("after connect:");

	return sk;
}

/*
 *  taken from BlueZ
 */
static int l2cap_accept(int sk, bdaddr_t *bdaddr)
{
	struct sockaddr_l2 addr;
	socklen_t addrlen;
	int nsk;

	fprintf(stderr, "Inside l2cap_accept socket %d\n", sk);

	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);

	if ((nsk = accept(sk, (struct sockaddr *) &addr, &addrlen)) < 0)
	{
		perror("accept returns error");
		return -1;
	}

	fprintf(stderr, "accept returns success\n");

	if (bdaddr)
		bacpy(bdaddr, &addr.l2_bdaddr);

	return nsk;
}



int main(int argc, char *argv[])
{
	unsigned char pkg[12];
	bdaddr_t dst;
	int mode = -1;
	char event_file[256] = {0};
	int optid;
	int retval;

	while ((optid = getopt(argc, argv, "hsc:e:")) != -1) {
		switch (optid) {
			case 's':
				mode = 0;
				//str2ba(optarg, &dst);
				break;
			case 'c':
				mode = 1;
				str2ba(optarg, &dst);
				break;
			case 'e':
				strcpy(event_file, optarg);
				break;
			default:
				//	usage();
				exit(0);
		}
	}

	printf("\nbtkbdemu DEM0 (c) Collin R. Mulliner <collin@betaversion.net> http://www.mulliner.org/bluetooth/\n\n");

	sdp_open();
	sdp_add_keyboard();
	printf("sdp record created\n\n");



	if (mode == 0) {
		cs = l2cap_listen(BDADDR_ANY, L2CAP_PSM_HIDP_CTRL, 0, 1);
		//cs = l2cap_listen(&dst, L2CAP_PSM_HIDP_CTRL, 0, 1);
		perror("HID listen control channel");
		is = l2cap_listen(BDADDR_ANY, L2CAP_PSM_HIDP_INTR, 0, 1);
		//is = l2cap_listen(&dst, L2CAP_PSM_HIDP_INTR, 0, 1);
		perror("HID listen interrupt channel");
		cs = l2cap_accept(cs, NULL);
		perror("HID control channel");
		is = l2cap_accept(is, NULL);
		perror("HID interupt channel");
	}
	else if (mode == 1) { 				// -c option. Connect to device
		cs = l2cap_connect(BDADDR_ANY, &dst, L2CAP_PSM_HIDP_CTRL);
		perror("HID control channel");
		is = l2cap_connect(BDADDR_ANY, &dst, L2CAP_PSM_HIDP_INTR);
		perror("HID interupt channel");
	}
	else {
		fprintf(stderr, "wrong mode\n");
		exit(-1);
	}
	printf("Connected ! ");

	int c;
	set_conio_terminal_mode();

	//while ((c = getch()) != 3) // 3 is ctrl z
	while ((c = getch()) != 0x7e) // 0x7e is F12 
	{
		printf("Input %x", c);

		pkg[0] = 0xa1;
		pkg[1] = 0x01;
		pkg[2] = 0x0;
		pkg[3] = 0x00;
		pkg[4] = 0x0; // the key code
		pkg[5] = 0x00;
		pkg[6] = 0x00;
		pkg[7] = 0x00;
		pkg[8] = 0x00;
		pkg[9] = 0x00;

		// a - z
		if ((c >= 0x61) && (c <= 0x7a))
		{
			pkg[4] = c - 0x5d;;
		}

		// A - Z
		else if ((c >= 0x41) && (c <= 0x5a))
		{
			pkg[2] = 0x2;
			pkg[4] = c - 0x3d;;
		}


		// 1 - 9
		else if ((c >= 0x31) && (c <= 0x39))
		{
			pkg[4] = c - 0x13;;
		}

		else if (c == 0x9) pkg[4] = 0x2b; 	// Tab 
		else if (c == 0x0d) pkg[4] = 0x28;

		else if (c == 0x20)  pkg[4] = 0x2c;  // space
		else if (c == 0x2d) pkg[4] = 0x2d; 	// - 
		else if (c == 0x2b)  	// + 
		{
			pkg[2] = 0x2;
			pkg[4] = 0x2e;;
		}

		else if (c == 0x22)  	// \" 
		{
			pkg[2] = 0x2;
			pkg[4] = 0x34;;
		}

		else if (c == 0x27)  pkg[4] = 0x34;  // \'

		else if (c == 0x2c) pkg[4] = 0x36; 	// , 
		else if (c == 0x2e) pkg[4] = 0x37; 	// . 
		else if (c == 0x2f) pkg[4] = 0x38; 	// / 

		else if (c == 0x3c)  	// < 
		{
			pkg[2] = 0x2;
			pkg[4] = 0x36;;
		}
		else if (c == 0x3d) pkg[4] = 0x2e; 	// = 

		else if (c == 0x3e)  	// > 
		{
			pkg[2] = 0x2;
			pkg[4] = 0x37;;
		}
		else if (c == 0x3f)  	// ? 
		{
			pkg[2] = 0x2;
			pkg[4] = 0x38;;
		}


		else if (c == 0x30)  pkg[4] = 0x27; 	// 0


		else if (c == 0x28) 		// (
		{
			pkg[2] = 0x2;
			pkg[4] = 0x26;;
		}
		else if (c == 0x29) 		// )
		{
			pkg[2] = 0x2;
			pkg[4] = 0x27;;
		}

		else if (c == 0x3a) 		// :
		{
			pkg[2] = 0x2;
			pkg[4] = 0x33;;
		}
		else if (c == 0x3b)  pkg[4] = 0x33; 	// ;

		else if (c == 0x5b) pkg[4] = 0x2f; // [
		else if (c == 0x5c) pkg[4] = 0x31; // back slash 

		else if (c == 0x5d) pkg[4] = 0x30; // ]

		else if (c == 0x5f) 		// _
		{
			pkg[2] = 0x2;
			pkg[4] = 0x2d;;
		}

		else if (c == 0x60) pkg[4] = 0x35; // `

		else if (c == 0x7b) 		// {
		{
			pkg[2] = 0x2;
			pkg[4] = 0x2f;;
		}
		else if (c == 0x7c) 		// | 
		{
			pkg[2] = 0x2;
			pkg[4] = 0x31;;
		}

		else if (c == 0x7d) 		// }
		{
			pkg[2] = 0x2;
			pkg[4] = 0x30;;
		}


		else if (c == 0x3)  // Ctrl C
		{
			pkg[2] = 0x1;
			pkg[4] = 0x63 - 0x5d;
		}

		else if (c == 0x13)  // Ctrl s
		{
			pkg[2] = 0x1;
			pkg[4] = 0x73 - 0x5d;
		}

		else if (c == 0x15)  // Ctrl u
		{
			pkg[2] = 0x1;
			pkg[4] = 0x75 - 0x5d;
		}

		else if (c == 0x16)  // Ctrl v
		{
			pkg[2] = 0x1;
			pkg[4] = 0x76 - 0x5d;
		}
		else if (c == 0x7f) pkg[4] = 0x2a;

		else if (c == 0x1b) 
		{
			if ((retval = kbhit()) == 1)
			{
				char d = getch();
				printf(" %x ", d);
				if ((d == 0x5b) || (d == 0x4f)) 
				{
					if ((retval = kbhit()) == 1)
					{
						char e = getch();
						printf(" %x ", e);
						if (e == 0x41) pkg[4] = 0x52; 	// Up 
						if (e == 0x42) pkg[4] = 0x51; 	// Down 
						if (e == 0x43) pkg[4] = 0x4f; 	// Right 
						if (e == 0x44) pkg[4] = 0x50; 	// Left 

						if (e == 0x5a)  // shift tab
						{							
							pkg[2] = 0x2; 	 
							pkg[4] = 0x2b; 	 
						}

						if (e == 0x33)
						{ 
							pkg[4] = 0x4c; 	// Delete 
							char e = getch(); // absorbing one 0x7e after delete. Avoid exit
							printf(" %x", e);
						}

					}
				}
			}
			else
			{
				printf("Single character escape");
				pkg[4] = 0x29; 	// Escape 
			}

		}

		else if (c == 0x3b) 
		{
			printf("Does this ever execute? \n\r");
			printf("Multiple characters \n\r");
			char d = getch();
			printf("Second char is %x \n\r", d);
			if (d == 0x32) 
			{
				char e = getch();
				printf("Third char is %x \n\r", d);

				if (e == 0x41) {pkg[2] = 0x2; pkg[4] = 0x52;}  	// Up 
				if (e == 0x42) {pkg[2] = 0x2; pkg[4] = 0x51;} 	// Down 
				if (e == 0x43) {pkg[2] = 0x2; pkg[4] = 0x4f;} 	// Right 
				if (e == 0x44) {pkg[2] = 0x2; pkg[4] = 0x50;} 	// Left 
			}

		}


		printf("\n\rpkg[2] pkg[4]:  %02x %02x\r\n", pkg[2], pkg[4]);

		if ((retval = write(is, pkg, 10)) <= 0) 
		{
			perror("write");
			exit(-1);
		}
		//printf("writing %d bytes\r\n", retval);



		pkg[0] = 0xa1;
		pkg[1] = 0x01;
		pkg[2] = 0;
		pkg[3] = 0x00;
		pkg[4] = 0; // Clear the key code
		pkg[5] = 0x00;
		pkg[6] = 0x00;
		pkg[7] = 0x00;
		pkg[8] = 0x00;
		pkg[9] = 0x00;

		if ((retval = write(is, pkg, 10)) <= 0) 
		{
			perror("write");
			exit(-1);
		}
		printf("Press F12 to exit\n\n\r");

	
	}

	printf("\r\nLast input was %x\r\n", c);
	printf("Closing Device\n");
	//	usb_release_interface(fdev,0);
	//	usb_close(fdev);

	return EXIT_SUCCESS;
}


// Change for playing around with git

void usage()
{
	fprintf(stderr,	"\nbtkbdemu v0.1 Dec. 2005 Collin R. Mulliner <collin@betaversion.net>\n\n"\
			"syntax: btkbdemu -hsce\n\n"\
			"\th\t\thelp\n"\
			"\ts\t\tserver mode\n"\
			"\tc <BD_ADDR>\tclient mode - connect to addr\n"\
			"\te <FILE>\tevent file\n");
}
