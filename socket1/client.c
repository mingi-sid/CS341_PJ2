#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#define DEBUG (1)
#define MAX_STR_SIZE (10485760)

unsigned int str2ip(char* ipadd); 
void print_param(char* host, char* port, char protocol);
uint16_t getChecksum(uint8_t op, uint8_t proto, uint32_t trans_id);

struct phase1 {
	uint8_t op;
	uint8_t proto;
	uint16_t checksum;
	uint32_t trans_id;
};

int main(int argc, char* argv[]) {
	srand(time(NULL));

	//Parameters
	char* param_host;
	char* param_port;
	char param_protocol = (char)-1;
	char label = '\0';
	for(int ii = 1; ii < argc; ii++) {
		if(argv[ii][0] == '-' && argv[ii][2] == '\0') {
			label = argv[ii][1];
			ii++;
			if(label == 'h') {
				param_host = argv[ii];
				if(str2ip(param_host) == (unsigned int)(-1)) {
					fprintf(stderr, "Parameter error : Not valid IP");
				}
			}
			else if(label == 'p') {
				param_port = argv[ii];
			}
			else if(label == 'm') {
				param_protocol = (char)(atoi(argv[ii]));
			}
			else {
				printf("Parameter error : Wrong format\n");
				return 0;
			}
		}
	}
	if(DEBUG==1) print_param(param_host, param_port, param_protocol);

	//Prepare to connect to server
	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo = NULL;
	struct addrinfo *p;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	if((status = getaddrinfo(param_host,param_port,&hints,&servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(1);
	}

	int soc;
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if(DEBUG == 1) printf("%x, %u\n",
		((struct sockaddr_in *)p->ai_addr)->sin_addr.s_addr, p->ai_addrlen);
		if((soc=socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			if(DEBUG) printf("1\n");
			perror("client: socket");
			continue;
		}

		/*
		if(bind(soc, p->ai_addr, p->ai_addrlen) == -1) {
	if(DEBUG) printf("1\n");
			perror("client: bind");
			continue;
		}
		*/

		if(connect(soc, p->ai_addr, p->ai_addrlen) == -1) {
	if(DEBUG) printf("1\n");
			close(soc);
			perror("client: connect");
			continue;
		}
		break;
	}

	if(p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return -1;
	}

	freeaddrinfo(servinfo);

	//Main loop. terminates when EOF is received.
	while(1) {
		//Get stdin input
		char *input_str;
		input_str = (char*)malloc(MAX_STR_SIZE);
		if(input_str == NULL) {
			fprintf(stderr, "Not enough memory");
			exit(1);
		}
		fgets(input_str, MAX_STR_SIZE, stdin);
		if(feof(stdin)) {
			break;
		}
		else if(input_str == NULL) {
			break;
		}
		if(DEBUG == 1) printf("echo : %s", input_str);

		//Phase 1 send
		/*
		 * 0      8       16           31
		 * |  op  | proto |  checksum  |
		 * |        trans_id           |
		 */
		struct phase1 phase1c;
		memset(&phase1c, 0, sizeof phase1c);
		phase1c.op = 0;
		phase1c.proto = (uint8_t)param_protocol;
		phase1c.trans_id = (uint32_t)rand();
		phase1c.checksum = getChecksum(phase1c.op, phase1c.proto, phase1c.trans_id);
		if(DEBUG == 1) printf("%u, %u, %u, %u\n", phase1c.op, phase1c.proto, phase1c.checksum, phase1c.trans_id);

		char p1c[8] = {0,};
		p1c[0] = phase1c.op;
		p1c[1] = phase1c.proto;
		p1c[2] = phase1c.checksum >> 8; p1c[3] = phase1c.checksum;
		p1c[4] = phase1c.trans_id >> 24; p1c[5] = phase1c.trans_id >> 16; 
		p1c[6] = phase1c.trans_id >> 8; p1c[7] = phase1c.trans_id;
		int send_return = send(soc, &p1c, sizeof p1c, 0);
		if(send_return == -1) {
			perror("send");
			exit(1);
		}
		else if(send_return != sizeof p1c) {
			if(DEBUG == 1) printf("Partial sending occurred\n");
			int sent = send_return;
			while(sent < sizeof p1c) {
				send_return = send(soc, &p1c + sent, sizeof p1c - sent, 0);
				if(send_return == -1) {
					perror("send");
					exit(1);
				}
				sent += send_return;
			}
		}

		//Phase 1 recv
		struct phase1 phase1s;
		memset(&phase1s, 0, sizeof phase1s);
		int recv_return = recv(soc, &phase1s, sizeof phase1s, 0);
		if(recv_return == -1) {
			perror("recv");
			exit(1);
		}
		else if(recv_return == 0) {
			fprintf(stderr, "Server connection closed\n");
			exit(1);
		}

		//Resolving received data
		if(phase1s.op != 1) {
			if(DEBUG == 1) printf("Server didn't send 1 as op\n");
			fprintf(stderr, "Illegal server respond\n");
			exit(1);
		}
		if(phase1s.proto != phase1c.proto) {
			if(DEBUG == 1) printf("Server protocol number does not match\n");
			fprintf(stderr, "Illegal server respond\n");
			exit(1);
		}
		if(phase1s.trans_id != phase1c.trans_id) {
			if(DEBUG == 1) printf("Server trans_id does not match\n");
			fprintf(stderr, "Illegal server respond\n");
			exit(1);
		}
		if(phase1s.checksum != getChecksum(phase1s.op, phase1s.proto, phase1s.trans_id)) {
			if(DEBUG == 1) printf("Server checksum is not valid\n");
			fprintf(stderr, "Illegal server respond\n");
			exit(1);
		}

		if(phase1s.proto == 1) {
			//Phase 2, protocol 1
			uint32_t input_str_len = strlen(input_str);
			char *phase2c;
			phase2c = (char *)malloc(1024 - 64);
			if(phase2c == NULL) {
				fprintf(stderr, "Not enough memory\n");
				exit(1);
			}
			uint32_t str_pos = 0;
			uint32_t phase2c_pos = 0;
			while(str_pos <= input_str_len) {
				if(str_pos == input_str_len || phase2c_pos == 1024 - 64 - 1) {
					if(str_pos == input_str_len) {
						phase2c[phase2c_pos] = '\\';
						phase2c_pos++;
						phase2c[phase2c_pos] = '0';
						phase2c_pos++;
						str_pos++;
					}
					else {
						phase2c_pos = 0;
					}
					//Send phase2c
					int send_p2 = send(soc, &phase2c, sizeof phase2c, 0);
					if(send_p2 == -1) {
						perror("send");
						exit(1);
					}
					else if(send_p2 != sizeof phase2c) {
						if(DEBUG == 1) printf("Partial sending occurred\n");
						int sent = send_p2;
						while(sent < sizeof phase2c) {
							send_return = send(soc, &phase2c + sent,
											   sizeof phase2c - sent, 0);
							if(send_p2 == -1) {
								perror("send");
								exit(1);
							}
							sent += send_p2;
						}
					}
				}
				else if(input_str[str_pos] == '\\') {
					phase2c[phase2c_pos] = '\\';
					phase2c_pos++;
					phase2c[phase2c_pos] = '\\';
					phase2c_pos++;
					str_pos++;
				}
				else {
					phase2c[phase2c_pos] = input_str[str_pos];
					phase2c_pos++;
					str_pos++;
				}
			}
			char *phase2s;
			phase2s = (char*)malloc(1024 - 64);
			if(phase2s == NULL) {
				fprintf(stderr, "Not enough memory\n");
				exit(1);
			}
			int recv_p2;
			while((recv_p2 = recv(soc, phase2s, 1024 - 64 - 1, 0)) != 0) {
				if(recv_p2 == -1) {
					perror("recv");
					exit(1);
				}
				phase2s[recv_p2] = '\0';
				fprintf(stdout, "%s", phase2s);
			}
		}
		else {
			fprintf(stdout, "Not Yet Implemented");
		}

	}
	//Main loop ends

	//Finish up
	close(soc);

	return 0;
}

/*
 * str2ip
 * Converts given string of "xxx.xxx.xxx.xxx"
 * to unsigned int.
 */
unsigned int str2ip(char* ipadd) {
	int cnt = 0;
	char* last_delim = 0;
	char* tmp = ipadd;
	unsigned int sum = 0;
	unsigned int total = 0;
	while(*tmp) {
		//if *tmp is not a number nor '.'
		if(*tmp >= '0' && *tmp <= '9') {
			sum = sum * 10 + (unsigned int)(*tmp - '0');
		}
		else if(*tmp == '.') {
			total = total * 256 + sum;
			sum = 0;
			cnt++;
		}
		else {
			return (unsigned int)(-1);
		}
		tmp++;
	}
	total = total * 256 + sum;
	if(cnt == 3) {
		return total;
	}
	else {
		return (unsigned int)(-1);
	}
}

/*
 * print_param
 * print the given parameters,
 * to check if they are okay
 */
void print_param(char* host, char* port, char protocol) {
	printf("Host : %s\n", host);
	printf("Port : %s\n", port);
	printf("Protocol : %u\n", protocol);
}
/*
 * getChecksum
 * calculate checksum from op and proto
 */
uint16_t getChecksum(uint8_t op, uint8_t proto, uint32_t trans_id) {
	uint16_t seg1 = op << 8 + proto;
	uint16_t seg2 = trans_id >> 16;
	uint16_t seg3 = trans_id;
	uint16_t checksum = 0;
	uint16_t sum = seg1 + seg2 + seg3;
	if((uint32_t)seg1 + (uint32_t)seg2 + (uint32_t)seg3 > 0xffffffff) {
		sum++;
	}
	checksum = ~sum;
	return checksum;
}

