#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG (1)

void print_param(unsigned int host, unsigned short port, char protocol);

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
int main(int argc, char* argv[]) {

	//Parameters
	unsigned int param_host = 0x00000000;
	unsigned short param_port = 0x0000;
	char param_protocol = (char)-1;
	char label = '\0';
	for(int ii = 1; ii < argc; ii++) {
		if(argv[ii][0] == '-' && argv[ii][2] == '\0') {
			if(DEBUG) printf("%c", argv[ii][1]);
			label = argv[ii][1];
			ii++;
			if(label == 'h') {
				param_host = str2ip(argv[ii]);
				if(param_host == (unsigned int)(-1)) {
					printf("Parameter error : Host IP\n");
				}
			}
			else if(label == 'p') {
				param_port = (unsigned short)(atoi(argv[ii]));
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
	if(DEBUG == 1) print_param(param_host, param_port, param_protocol);
	return 0;
}
/*
 * print_param
 * print the given parameters,
 * to check if they are okay
 */
void print_param(unsigned int host, unsigned short port, char protocol) {
	int ip[4] = {0, };
	ip[3] = host & 0xff;
	ip[2] = (host >> 8) & 0xff;
	ip[1] = (host >> 16) & 0xff;
	ip[0] = (host >> 24) & 0xff;
	printf("Host : %u.%u.%u.%u(%x)\n", ip[0], ip[1], ip[2], ip[3], host);
	printf("Port : %u\n", port);
	printf("Protocol : %u\n", protocol);
}
