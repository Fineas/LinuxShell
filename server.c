#include <stdio.h>
#include <string.h>  
#include <stdlib.h>   
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <unistd.h>   
#include <pthread.h>

void display_prompt(int fd){
    // dprintf(fd,"\033[36m\033[40m"); 
    // dprintf(fd," 🐚 ");
    // dprintf(fd,"\033[0m");
    // dprintf(fd,"\033[37m\033[46m"); 
    // dprintf(fd," user@local_session ");
    // dprintf(fd,"\033[0m");
    // dprintf(fd,"\033[36m\033[40m"); 
    // dprintf(fd," > ");
    // dprintf(fd,"\033[0m");  

    dprintf(fd, "$> \x00");          
}

void *connection_handler(void *socket_desc){
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    int read_size;
    char client_message[2000];

    display_prompt(sock);

    //Receive a message from client
    while( (read_size = recv(sock , client_message , 2000 , 0)) > 0 ){

        //end of string marker
		client_message[read_size] = '\0';
		
		int pid;

        if ((pid = fork()) == 0){
            dup2(sock, STDOUT_FILENO);
            dup2(sock, STDERR_FILENO);

            char **args = malloc( sizeof(char *[3]));
            args[0] = (char *)"a.out";
            args[1] = client_message;
            args[2] = NULL;

            execv(args[0], args);
            fprintf(stderr, "[!] Command '%s' not found!\n", args[0]);
            exit(EXIT_FAILURE);
        }
        waitpid(pid, NULL, 0);


		//clear the message buffer
		memset(client_message, 0, 2000);
        display_prompt(sock);
    }
     
    if(read_size == 0){
        puts("[*] Client disconnected.");
        fflush(stdout);
    }
    else if(read_size == -1){
        perror("[!] Recv failed.");
    }
         
    return 0;
} 
 
int main(int argc , char *argv[]){
    int socket_desc , client_sock , c;
    struct sockaddr_in server , client;
     
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1){
        puts("[!] Could not create socket.");
    }
    puts("[*] Socket created.");
     
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 31337 );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0){
        //print the error message
        perror("[!] bind failed. Error");
        return 1;
    }
    puts("[*] Bind done.");
     
    //Listen
    listen(socket_desc , 3);
     
    //Accept and incoming connection
    puts("[*] Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
     
    //Accept and incoming connection
    puts("[*] Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
	pthread_t thread_id;
	
    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ){
        puts("[*] Connection accepted.");
         
        if( pthread_create( &thread_id , NULL ,  connection_handler , (void*) &client_sock) < 0){
            perror("[!] Could not create thread.");
            return 1;
        }
         
        puts("[*] Handler assigned.");
    }
     
    if (client_sock < 0){
        perror("[!] Accept failed.");
        return 1;
    }
     
    return 0;
}