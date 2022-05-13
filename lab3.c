#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <libgen.h>

#define MEMSIZE 30
#define CWD "CWD /work\r\n"

char clientIP[15];
char *serverAnswer;
int commandSocket, dataSocket, connectionFlag = 0;

void getServerAnswer(int sock, char *buf) {
	int bytesRead;

	bytesRead = recv(sock, buf, 1024, 0);

	write(1, buf, bytesRead);
	putchar('\n');
}

int makeConnection(int *sock, char *IP, char *port) {
	struct addrinfo *info, hints;

	*sock = socket(AF_INET, SOCK_STREAM, 6);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	if (getaddrinfo(IP, port, &hints, &info)) {
		write(1, "Error!That's not IP!\n", 21);
		return 1;
	}

	connect(*sock, info -> ai_addr, info -> ai_addrlen);
	printf("connection port: %s\n", port);

	return 0;
}

int commandTransferConnect() {
	char serverIP[MEMSIZE];
	char *login, *loginToServer; 
	char *password, *passwordToServer;

	scanf("%s", serverIP);

	if (makeConnection(&commandSocket, serverIP, "21"))
		return 1;
	
	getServerAnswer(commandSocket, serverAnswer);

	if (strncmp(serverAnswer, "220", 3) == 0) {
		printf("Write your login: ");

		login = (char*) malloc(MEMSIZE * sizeof(char));
		scanf("%s", login);
		loginToServer = (char*) malloc((strlen(login) + 7) * sizeof(char));
		sprintf(loginToServer, "USER %s\r\n", login);

		send(commandSocket, loginToServer, strlen(loginToServer) * sizeof(char), 0);
		getServerAnswer(commandSocket, serverAnswer);

		free(login);
		free(loginToServer);
		
		if (strncmp(serverAnswer, "331", 3) == 0) {
			printf("Write your password: ");

			password = (char*) malloc(MEMSIZE * sizeof(char));
			scanf("%s", password);
			passwordToServer = (char*) malloc((strlen(password) + 7) * sizeof(char));
			sprintf(passwordToServer, "PASS %s\r\n", password);
			
			send(commandSocket, passwordToServer, strlen(passwordToServer) * sizeof(char), 0);
			getServerAnswer(commandSocket, serverAnswer);

			free(password);
			free(passwordToServer);

			if (strncmp(serverAnswer, "230", 3) != 0) {
				write(1, "Error!That's not password!\n", 27);
				return 1;
			}
		}
		else {
			write(1, "Error!That's not login!\n", 24);
			return 1;
		}
	}

	return 0;
}	

int dataTransferConnect() {
	char *tmp_char;
	int *mas = (int*) calloc(6, sizeof(int));	
	char *dataServerIP = (char*) malloc(15 * sizeof(char));
	char *dataServerPort = (char*) malloc(5 * sizeof(char));

	send(commandSocket, "TYPE I\r\n", 8 * sizeof(char), 0);
	getServerAnswer(commandSocket, serverAnswer);
	write(1, "\033[1A", 5);

	if (strncmp(serverAnswer, "200", 3) == 0) {
		send(commandSocket, "PASV\r\n", 6 * sizeof(char), 0);
		getServerAnswer(commandSocket, serverAnswer);
		write(1, "\033[1A", 5);

		if (strncmp(serverAnswer, "227", 3) == 0) {
			tmp_char = strtok(serverAnswer, "(");
			tmp_char = strtok(NULL, "(");
			tmp_char = strtok(tmp_char, ").");

			sscanf(tmp_char, "%d,%d,%d,%d,%d,%d", &mas[0], &mas[1], &mas[2], &mas[3], &mas[4], &mas[5]);

			sprintf(dataServerIP, "%d.%d.%d.%d", mas[0], mas[1], mas[2], mas[3]);
			sprintf(dataServerPort, "%d", mas[4] * 256 + mas[5]);

			if (makeConnection(&dataSocket, dataServerIP, dataServerPort))
				return 1;
			putchar('\n');
		}
		else {
			write(1, "Error!Passive mode is not entered!\n", 27);
			return 1;
		}

	}
	else {
		write(1, "Error!Type is not changed!\n", 27);
		return 1;
	}

	free(mas);
	free(dataServerIP);
	free(dataServerPort);

	return 0;
}

void cd() {
	send(commandSocket, CWD, 11 * sizeof(char), 0);
	write(1, CWD, 11);
	getServerAnswer(commandSocket, serverAnswer);
}

int getnput(int flag) {
	FILE *fd;
	char *retrstor, *fileName, *buf;
	int bytesRead;

	retrstor = (char*) malloc(MEMSIZE * sizeof(char));
	fileName = (char*) malloc(MEMSIZE * sizeof(char));
	buf = (char*) malloc(1024 * sizeof(char));

	scanf("%s", fileName);
	if (flag)
		sprintf(retrstor, "RETR %s\r\n", fileName);
	else 
		sprintf(retrstor, "STOR %s\r\n", fileName);

	send(commandSocket, retrstor, strlen(retrstor) * sizeof(char), 0);
	getServerAnswer(commandSocket, serverAnswer);
	write(1, "\033[1A", 5);

	if (strncmp(serverAnswer, "150", 3) == 0) {
		if (flag) {
			char *tmp_size;
			int file_size;
			int read = 0;

			tmp_size = strtok(serverAnswer, "(");
			tmp_size = strtok(NULL, "(");
			tmp_size = strtok(tmp_size, " ");
			sscanf(tmp_size, "%d", &file_size);

			fd = fopen(fileName, "wb");

			do {
				bytesRead = recv(dataSocket, buf, sizeof(buf), 0);

				fwrite(buf, 1, bytesRead, fd);
				
				read += bytesRead;
			} while (read < file_size);
		}
		else {
			fd = fopen(fileName, "rb");

			while (1) {
				bytesRead = fread(buf, 1, 1024, fd);
	
				if (bytesRead <= 0)
					break;

				send(dataSocket, buf, bytesRead, 0);
			}
		}

		fclose(fd);
		close(dataSocket);

		getServerAnswer(commandSocket, serverAnswer);
	}
	else {
		write(1, "Error!No such file!\n", 20);
		return 1;
	}
	free(retrstor);
	free(fileName);
	free(buf);

	return 0;
}

void setClientIP(char *clientIP) {
	struct ifaddrs *ptr, *netInterfaces;
	int family;
	char host[NI_MAXHOST];

	getifaddrs(&netInterfaces);

	for (ptr = netInterfaces; ptr != NULL; ptr = ptr -> ifa_next) {
		family = ptr -> ifa_addr -> sa_family;

		if (family == AF_INET) {
			getnameinfo(ptr -> ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

			if (strcmp(ptr -> ifa_name, "lo") != 0) {
				sprintf(clientIP, "%s", host);
				return;
			}
		}
	}

	freeifaddrs(netInterfaces);
}

int getCommand() {
	char command[MEMSIZE];

	write(1, "ftp> ", 5);
	scanf("%s", command);

	if (!strcmp(command, "connect")) {
		if (commandTransferConnect())
			return 0;
		connectionFlag = 1;
	}
	else 
		if (!strcmp(command, "quit"))
			return 0;
		else
			if (connectionFlag) {
				if (!strcmp(command, "cd")) {
					cd();
				}
				else 
					if (!strcmp(command, "get")){
						if (dataTransferConnect())
							return 0;
						if (getnput(1))
							return 0;
					}
					else 
						if (!strcmp(command, "put")) {
							if (dataTransferConnect())
								return 0;
							if (getnput(0))
								return 0;
						} 
			}

	return 1;
}

int main(int argc, char* argv[]) {
	setClientIP(clientIP);
	serverAnswer = (char*) malloc(1024 * sizeof(char));

	while (getCommand());

	if (connectionFlag) {
		send(commandSocket, "QUIT\r\n", 6 * sizeof(char), 0);
		getServerAnswer(commandSocket, serverAnswer);
		close(commandSocket);
	}
	free(serverAnswer);

	return 0;
}
