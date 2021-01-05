/*
	C ECHO client example using sockets
*/
#include <stdio.h>	//printf
#include <string.h>	//strlen
#include <sys/socket.h>	//socket
#include <arpa/inet.h>	//inet_addr
#include <unistd.h>

int main(int argc , char *argv[])
{
	int sock, r = 0;
	struct sockaddr_in server;
	char message[1000] , server_reply[2000];
	
	// Create socket
	sock = socket(AF_INET , SOCK_STREAM , 0);
	if (sock == -1){
		printf("Could not create socket");
	}
	puts("[*] Socket created");
	
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons( 31337 );

	// Connect to remote server
	if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0){
		perror("connect failed. Error");
		return 1;
	}

    puts("[*] Connected\n");

    // get Shell prompt
    if( recv(sock , server_reply , 2000 , 0) < 0){
        puts("recv failed");
    }
    puts(server_reply);

	
	while(1){
		fgets(message, 999, stdin);
		
		// Send command
		if( send(sock , message , strlen(message) , 0) < 0){
			puts("Send failed");
			return 1;
		}
		
		// Get response from command
		if(recv(sock , server_reply , 2000 , 0) > 0){
			printf("%s",server_reply);
		}

        memset(server_reply, 0, 2000);

        // Get prompt
        if(recv(sock , server_reply , 2000 , 0) > 0){
			printf("%s",server_reply);
		}

        memset(server_reply, 0, 2000);
        memset(message, 0, 1000);
		
		
	}
	
	close(sock);
	return 0;
}