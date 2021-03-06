#include "packet.h"
#include "timer.h"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

// ============== Prototypes =============
void setup_connection(char*, int);
bool DATisUnique(struct packet*);
void store_dataPacket(struct packet*);
void write_file(char*);
void print_log(bool, bool, int, int, int);
char* get_adrString(struct in_addr, in_port_t);
void print_statistics();
void terminate(int);

// =============== Globals ===============
/* List of data packets received so far */
struct packet** data_packets;
int window_size;

/* Connection globals */
int socketfd;
struct sockaddr_in adr_sender;
struct sockaddr_in adr_receiver;

/* Statistics globals */
int DAT_bytes_received;
int unique_DAT_bytes_received;
int DAT_packets_received;
int unique_DAT_packets_received;
int SYN_packets_received;
int FIN_packets_received;
int RST_packets_received;
int ACK_packets_sent;
int RST_packets_sent;
// =======================================

int main(int argc, char* argv[])
{
	char* receiver_ip;
	int receiver_port;
	char* filename;

	if(argc != 4)
	{
		printf("Usage: $./rdpr receiver_ip receiver_port filename\n");
		exit(0);
	}

	receiver_ip = argv[1];
	receiver_port = atoi(argv[2]);
	filename = argv[3];

	setup_connection(receiver_ip, receiver_port);

	 /*// Send test packet
	char test_data[MAX_PAYLOAD_SIZE] = "this is a test packet";
	struct packet* test_pckt = create_packet(test_data, MAX_PAYLOAD_SIZE, DAT, 69);
	send_packet(test_pckt, socketfd, adr_sender);
	*/
	data_packets = (struct packet**)calloc(MAX_PACKETS, sizeof(struct packet*));
	while(1)
	{
		// Receive incoming packets
		struct sockaddr_in adr_src;
		//printf("receiving...\n");
		struct packet* rec_pckt = receive_packet(socketfd, &adr_src);
		
		if(rec_pckt != NULL)
		{
			if(rec_pckt->header.type == DAT)
			{
				//printf("received DAT\n");
				// Send ACK
				add_packet(rec_pckt, data_packets);
				struct packet* ACK_pckt = create_packet(NULL, 0, ACK, rec_pckt->header.ackno);
				send_packet(ACK_pckt, socketfd, adr_src);
				print_log(true, false, ACK, rec_pckt->header.seqno, 0);
				// update statistics
				ACK_packets_sent++;
				DAT_bytes_received += rec_pckt->payload_length;
				DAT_packets_received++;
				if(DATisUnique(rec_pckt))
				{
					unique_DAT_bytes_received += rec_pckt->payload_length;
					unique_DAT_packets_received++;
				}
				// store data packet in the global list
				store_dataPacket(rec_pckt);
			}
			else if(rec_pckt->header.type == SYN)
			{
				// Send ACK
				struct packet* ACK_pckt = create_packet(NULL, 0, ACK, rec_pckt->header.ackno);
				send_packet(ACK_pckt, socketfd, adr_src);
				print_log(true, false, ACK, rec_pckt->header.seqno, 0);
				// update statistics
				SYN_packets_received++;
				ACK_packets_sent++;
			}
			else if(rec_pckt->header.type == RST)
			{
				RST_packets_received++;
				terminate(-1);
			}
			// Sender is done transferring
			else if(rec_pckt->header.type == FIN)
			{
				//send ACK and break to begin writing to file
				struct packet* ACK_pckt = create_packet(NULL, 0, ACK, rec_pckt->header.ackno);
				send_packet(ACK_pckt, socketfd, adr_src);
				print_log(true, false, ACK, rec_pckt->header.seqno, 0);
				// update statistics
				FIN_packets_received++;
				ACK_packets_sent++;
				break;
			}
			if(clock()/CLOCKS_PER_SEC >= RECEIVER_TIMEOUT_S)
			{
				terminate(-2);
			}
			print_log(false, false, rec_pckt->header.type, rec_pckt->header.seqno, rec_pckt->payload_length);
		}
	}
	// should have all data packets stored
	print_statistics();
	write_file(filename);
	return 0;
}

/*
 * Create socket and sender/receiver addresses and bind to socket
 */
void setup_connection(char* receiver_ip, int receiver_port)
{
	//	 Create UDP socket
	if((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("receiver: socket()\n");
		exit(-1);
	}
	// Set timeout value for the recvfrom
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
	{
		perror("setsockopt():");
		exit(-1);
	}
	// Create sender address
	memset(&adr_sender,0,sizeof adr_sender);
	adr_sender.sin_family = AF_INET;
	//adr_sender.sin_port = htons(8080);
	adr_sender.sin_port = htons(2699);
	adr_sender.sin_addr.s_addr = inet_addr("192.168.1.100");
	//adr_sender.sin_addr.s_addr = inet_addr("142.104.74.69");
	//adr_sender.sin_addr.s_addr = inet_addr("127.0.0.1");
	// Create receiver address
	memset(&adr_receiver,0,sizeof adr_receiver);
	adr_receiver.sin_family = AF_INET;
	adr_receiver.sin_port = htons(receiver_port);
	adr_receiver.sin_addr.s_addr = inet_addr(receiver_ip);
	// Error check addresses
	if ( adr_sender.sin_addr.s_addr == INADDR_NONE ||
		 adr_receiver.sin_addr.s_addr == INADDR_NONE )
	{
		perror("receiver: created bad address\n)");
		exit(-1);
	}
	// Bind to socket
	if(bind(socketfd, (struct sockaddr*)&adr_receiver,  sizeof(adr_receiver)) < 0)
	{
		perror("receiver: bind()\n");
		exit(-1);
	}
	printf("receiver: running on %s:%d\n",inet_ntoa(adr_receiver.sin_addr), ntohs(adr_receiver.sin_port));
}

/*
 * Store the data packet in the global list of data packets stored so far
 */
void store_dataPacket(struct packet* pckt)
{
	int i;
	for(i=0; data_packets[i] != 0; i++)
	{
		// if packet has already been stored, ignore it
		if(data_packets[i]->header.seqno == pckt->header.seqno)
			break;
	}
	// add the packet to the end of the list
	data_packets[i] = pckt;
}

/*
 * Check if data packet is unique and not a re-send
 */
bool DATisUnique(struct packet* pckt)
{
	int i;
	for(i=0; data_packets[i] != 0; i++)
	{
		if(data_packets[i]->header.seqno == pckt->header.seqno)
			return true;
	}
	return false;
}

/*
 * Write the data packets to the output file
 */
void write_file(char* filename)
{
	// Open file
	FILE* file;
	if((file = fopen(filename, "w")) == NULL)
	{
		perror("receiver: open\n");
		exit(-1);
	}
	// write every data packet
	int i;
	for(i=0; data_packets[i] !=0 ;i++)
	{
		fwrite(data_packets[i]->payload, 1, sizeof(data_packets[i]->payload), file);
	}
	printf("DONE WRITING TO FILE: %s\n",filename);
	fclose(file);
}

/*
 * Real-time log as packets are sent and received
 */
void print_log(bool sent, bool unique, int typeno, int no, int length)
{
	char* type = type_itos(typeno);
	char* source_adr;
	char* dest_adr;
	char* current_time = (char*)malloc(sizeof(current_time)*100);
	char* event_type = (char*)malloc(sizeof(event_type)*10);
	char* seqackno = (char*)malloc(sizeof(seqackno)*10);
	char* payloadwindow = (char*)malloc(sizeof(payloadwindow)*10);
	char* full_log = (char*)malloc(sizeof(full_log)*200);

	sprintf(seqackno, "%d", no);
	sprintf(payloadwindow, "%d", length);
	if(sent)
	{
		sprintf(event_type, "%s", KGRN);
		if(unique)
		{
			strcat(event_type, "s");
		}
		else
		{
			strcat(event_type, "S");
		}
		strcat(event_type, KNRM);
		source_adr = get_adrString(adr_sender.sin_addr, adr_sender.sin_port);
		dest_adr = get_adrString(adr_receiver.sin_addr, adr_receiver.sin_port);
	}
	else
	{
		sprintf(event_type, "%s", KCYN);
		if(unique)
			strcat(event_type, "r");
		else
			strcat(event_type, "R");
		strcat(event_type, KNRM);
		dest_adr = get_adrString(adr_sender.sin_addr, adr_sender.sin_port);
		source_adr = get_adrString(adr_receiver.sin_addr, adr_receiver.sin_port);
	}
	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	current_time = asctime (timeinfo);
	// stip newline from current time
	int i;
	for(i=0; current_time[i] != '\0'; i++)
	{
		if(current_time[i] == '\n')
			current_time[i] = '\0';
	}
	//printf ( "Current local time and date: %s\n", current_time );
	// cat them all together
	char space[2] = " ";
	strcpy(full_log, current_time);
	strcat(full_log, space);
	strcat(full_log, event_type);
	strcat(full_log, space);
	strcat(full_log, source_adr);
	strcat(full_log, space);
	strcat(full_log, dest_adr);
	strcat(full_log, space);
	strcat(full_log, type);
	strcat(full_log, space);
	strcat(full_log, seqackno);
	strcat(full_log, space);
	strcat(full_log, payloadwindow);
	printf("%s\n",full_log);
}

/*
 * Construct and return an address string for the sake of logging
 */
char* get_adrString(struct in_addr adrIp, in_port_t adrPort)
{
	char* ip = inet_ntoa(adrIp);
	int pt = ntohs(adrPort);
	char* strport = (char*)malloc(sizeof(strport)*10);
	sprintf(strport, "%d", pt);
	strcat(ip, ":");
	strcat(ip, strport);
	return ip;
}

/*
 * Log receiver statistics
 */
void print_statistics()
{
	printf("Receiver Statistics:\n");
	printf(" Total data bytes received: %d\n",DAT_bytes_received);
	printf(" Unique data bytes received: %d\n",unique_DAT_bytes_received);
	printf(" Total data packets received: %d\n",DAT_packets_received);
	printf(" Unique data packets received: %d\n",unique_DAT_packets_received);
	printf(" SYN packets received: %d\n",SYN_packets_received);
	printf(" FIN packets received: %d\n",FIN_packets_received);
	printf(" RST packets received: %d\n",RST_packets_received);
	printf(" ACK packets sent: %d\n",ACK_packets_sent);
	printf(" RST packets sent: %d\n",RST_packets_sent);
	clock_t time = clock();
	double int_time = (double)time/CLOCKS_PER_SEC;
	printf(" Transfer duration: %f\n",int_time);
}

/*
 * Terminate the transfer process with failure or success
 * param code: <0 failure (RST), >=0 success (FIN)
 */
void terminate(int code)
{
	if(code == -1)
	{
		perror("receiver: terminating with failure (RST)\n");
		exit(-1);
	}
	else if(code == -2)
	{
		perror("receiver: terminating, transfer took too long\n");
		exit(-1);
	}
	printf("receiver: terminating with success (FIN)\n");
	exit(0);
}
