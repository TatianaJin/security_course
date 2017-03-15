How to restore the telnet session after hijack:

1. The present situation after evil data are sent:
  - the sequence number of client is smaller than the ACK number of server.
  - the ACK number of client is smaller than the sequence number of server.
  - the client keeps sending a packet containing the keystroke that is not ACKed by the server.
  - the server replies a ack packet with the correct sequence number (according to the received ACK number of client) but a larger ACK number.
  
2. The task is to correct the sequence number of the client so that it is equal to the ACK number of server
  - the client sends one packet a time for one keystroke, a new packet will be sent only after the last packet is ACKed.
  - for each packet p sent from the client, the hacker send an ACK packet with the corresponding sequence number (= p's ACK) and ACK number (= p's SEQ + p's data size), until the session is restored
  - when the client aggressively types the keyboard, it will send many packets following the above-mentioned pattern, and consequently the ACK and SEQ numbers of the client and the server match again
