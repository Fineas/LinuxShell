#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <readline/readline.h>
#include <readline/history.h>

#define BUFSIZE 2048
#define MAXARGS 16
#define SPLITTERS " \t\r\n\a"
#define BUF_SIZE 1024
#define DIR_BUF_SIZE 256

extern int errno;

int change_directory(char **args);
int display_history(char **args);
int create_directory(char **args);
int handle_pipe_command(char *line);
int help_function(char **args);
int exit_function(char **args);
void *connection_handler(void *);
int copy(char src[], char dst[], char root[], int interactive, int verbose, int recursive, int suffix);
int explore_dir(char src[], char dst[], char root[], int interactive, int verbose, int recursive, int suffix);

enum { MAXC = 128 };
char ps[MAXC];
char *custom_commands[] = {"cd","help","exit","history","mkdir","version","dirname","cp","mv"};
int counter=0;
int new_path=0;

// Function to extract Command Arguments
char **splitline(char *line){
    int buff = MAXARGS, position = 0;
    char **tokens = malloc(buff * sizeof(char*));
    char *token, **tokens_backup;

    if (!tokens) {
        printf("[!] Dynamic memory allocation error.\n");
        exit(0);
    }

    token = strtok(line, SPLITTERS);
    while (token != NULL) {
        if(token[0] == '*'||token[strlen(token)-1] == '*'){
            DIR *d;
            struct dirent *dir;
            d = opendir(".");
            if (d) {
                while ((dir = readdir(d)) != NULL) {
                    if(dir->d_name[0]!='.'){
                        if(strlen(token) == 1 ||
                            (token[0] == '*' && (!strncmp(token+1,(dir->d_name+strlen(dir->d_name)-strlen(token)+1),strlen(token)-1))) || 
                            (token[strlen(token)-1] == '*' && (!strncmp(token,dir->d_name,strlen(token)-1)))){
                            tokens[position] = dir->d_name;
                            position++;
                        }
                    }
                }
                closedir(d);
            }
        }

        else{tokens[position] = token;
        position++;}

        if (position >= buff) {
            buff += MAXARGS;
            tokens_backup = tokens;
            tokens = realloc(tokens, buff * sizeof(char*));
            if (!tokens) {
                free(tokens_backup);
                printf("[!] Dynamic memory allocation error.\n");
                exit(0);
            }
        }
        token = strtok(NULL, SPLITTERS);
    }
    tokens[position] = NULL;
    return tokens;
}

// Function to handle Directory Change
int change_directory(char **args){ 
    if (args[1] == NULL) {
        if(chdir(getenv("HOME"))!= 0){
            printf("[!] Change directory failed.\n");
            return 3;
        }
    } 
    else {
        if (chdir(args[1]) != 0) {
            printf("[!] Change directory failed.\n");return 3;
        }
    }
    return 1;
}

// Function to handle Command History
int display_history(char **args){
    if (args[1] != NULL) {
        printf("[!] Unrequired argument to \"history\"\n");return 3;
    } 

    /* get the state of your history list (offset, length, size) */
    HISTORY_STATE *myhist = history_get_history_state ();

    /* retrieve the history list */
    HIST_ENTRY **mylist = history_list ();

    for (int i = 0; i < myhist->length; i++) { /* output history list */
        printf (" %8s  %s\n", mylist[i]->line, mylist[i]->timestamp);
        free_history_entry (mylist[i]);     /* free allocated entries */
    }
    putchar ('\n');

}

// Function to handle exit
int exit_function(char **args){
    if (args[1] == NULL) {
        asm("mov rax, 60;");
        asm("mov rdi, 0x0");
        asm("syscall");
    } 
    else{
        printf( "Exit requires no arguments\n");return 3;
    }
}

// Function to handle the dirname command
int dirname_function(char **args){
    if(args[1] != NULL){
        char *dirc, *dname;

        dirc = strdup(args[1]);
        dname = dirname(dirc);

        printf("%s\n", dname);
        return 1;
    }
    else{
        printf( "Missing operand.\n");return 3;
    }
}

int copy(char src[], char dst[], char root[], int interactive, int verbose, int recursive, int suffix){
	int n, in, out;
	char buf[BUF_SIZE];

	chdir(root);

	if((in = open(src, O_RDONLY)) < 0){
		printf("[!] Error\n");
		printf("%s",src);
		return 3;
	}

    if(interactive){
        if( access( dst, F_OK ) == 0 ) {
            printf("[!] Attention: overwrite '%s'? (y/n)", dst);
            char ans;
            scanf("%c",&ans);
            if(ans != 'y'){
                return 1;
            }
        }
    }

    // create backup file
    if(suffix){
        if( access( dst, F_OK ) == 0 ) {
            puts("doing backup thingy...");
            char ch;
            FILE *source, *target;
            source = fopen(dst, "r");
            char target_file[100];
            memset(target_file, 0, 100);
            strcpy(target_file,dst);
            strcat(target_file,".txt\x00");
            target = fopen(target_file, "w");

            if( source == NULL  || target == NULL){
                printf("[!] Error\n");
                return 3;
            }

            while( ( ch = fgetc(source) ) != EOF )
                fputc(ch, target);

            fclose(source);
            fclose(target);
        }
    }

	if((out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0){
		printf("[!] Copy open error.\n");
		printf("%s",dst);
		return 3;
	}

    if(verbose){
        // 'file1' -> 'file2'
        printf("'%s' -> '%s'\n", src, dst);
    }
	while((n = read(in, buf, sizeof(buf))) > 0)
		write(out, buf, n);

	close(out);
	close(in);
	
	return 1;
}

int explore_dir(char src[], char dst[], char root[], int interactive, int verbose, int recursive, int suffix){
	DIR *pdir;
	struct dirent *dirt;
	struct stat st;

	char *dir_name[DIR_BUF_SIZE];
	char tmp_src[DIR_BUF_SIZE];
	char tmp_dst[DIR_BUF_SIZE];
	char tmp_dst_dir[DIR_BUF_SIZE];

	int i = 0, count = 0, f_count = 0;

	strcpy(tmp_src, src);
	strcpy(tmp_dst, dst);
	strcat(tmp_src, "/");
	strcat(tmp_dst, "/");

	chdir(root);

	if((pdir = opendir(src)) <= 0){
		printf("opendir");
		return 3;
	}

	chdir(src);
	
	while((dirt = readdir(pdir)) != NULL){

		lstat(dirt->d_name, &st);

		if(S_ISDIR(st.st_mode)){
			if(strcmp(dirt->d_name, ".") && strcmp(dirt->d_name, "..")){
				dir_name[count] = dirt->d_name;
				count++;
			}
		}
		else if(S_ISREG(st.st_mode)){
			strcat(tmp_src, dirt->d_name);
			strcat(tmp_dst, dirt->d_name);
			
			chdir(root);
			copy(tmp_src, tmp_dst, root, interactive, verbose, recursive, suffix);
			chdir(src);

			strcpy(tmp_src, src);
			strcpy(tmp_dst, dst);
			strcat(tmp_src, "/");
			strcat(tmp_dst, "/");
		}
	}

    // go deeper only if the recursive tag is attached
	for(i = 0; i < count; i++){
        if(recursive){
            strcpy(tmp_src, src);
            strcat(tmp_src, "/");
            strcat(tmp_src, dir_name[i]);

            strcpy(tmp_dst, dst);
            strcat(tmp_dst, "/");
            strcat(tmp_dst, dir_name[i]);

            strcpy(tmp_dst_dir, root);
            strcat(tmp_dst_dir, "/");
            strcat(tmp_dst_dir, tmp_dst);

            chdir(root);
            if(mkdir(tmp_dst, S_IRUSR | S_IWUSR | S_IXUSR) < 0){
                printf("[!] Error mkdir.\n");
                return 3;
            }
            chdir(src);
            
            if(explore_dir(tmp_src, tmp_dst, root, interactive, verbose, recursive, suffix) == -1){
                break;
            }
        }
        else{
            printf("[!] Attention: -r not specified; omitting directory '%s'.\n",dir_name[i] );
        }
	}

	closedir(pdir);
	chdir("..");

	return 1;
}


// Function to handle the cp command
int copy_function(char **args){
    if(args[1] == NULL){
        printf("[!] Missing operands.\n");return 3;
    }
    else{
        /*
            -v
            '../f1/file1' -> './file1'
            '../f1/file2' -> './file2'

            -i
            cp: overwrite './file1'? y

            -r == copy directories recursively
            cp: -r not specified; omitting directory 'geeksforgeeks' (error)

            -t, --target-directory=DIRECTORY  copy all SOURCE arguments into DIRECTORY
        */

        int verbose = 0;
        int interactive = 0;
        int recursively = 0;
        char* target_dir=(char*)malloc((128) * sizeof(char));
        int target = 0, i, src_is_reg, dst_is_reg = 0, dst_exist = 1, mode;
        struct stat st_src, st_dst;
        int arg_cnt = 1;
        int opt; 
        
        do{
            arg_cnt++;
        }
        while( args[arg_cnt] != NULL );


        while ((opt = getopt(arg_cnt, args, "vrit:")) != -1) {
            switch(opt)  {  
                case 'v':  
                    verbose = 1;
                    break;
                case 'r':  
                    recursively = 1;
                    break;
                case 'i':   
                    interactive = 1;
                    break;  
                case 't':  
                    target = 1;
                    strcpy(target_dir, optarg+1); 
                    break;  
                case '?':  
                    printf("[!] Unknown option: %c\n", optopt); 
                    break;  
            }  
        } 

        if(target == 0){
            if(arg_cnt - optind < 2){
                printf("[!] Missing operands (source / destination).\n");return 3;
            }
        }

        // ================================ DO ACTUALY COPY

        char root[DIR_BUF_SIZE];
        getcwd(root, DIR_BUF_SIZE);

        char *dst;
        if(target){
            dst = target_dir;
            printf(">> %s \n", dst);
        }
        else{
            dst = malloc(strlen(args[arg_cnt-1])+1); // destination
            strcpy(dst, args[arg_cnt-1]);
        }

        for(int j = optind; j < arg_cnt-1; j++){
            char *src = args[j]; // source
            strcpy(dst, args[arg_cnt-1]);

            if(stat(src, &st_src) == -1){ // check if source exists
                printf("[!] src stat failed.");
                return 3;
            }

            if(stat(dst, &st_dst) == -1){ // check if destination exists
                dst_exist = 0;
            }

            // check if source is regular file
            if(S_ISREG(st_src.st_mode)){
                src_is_reg = 1;
            }
            // check if source is directory
            else if(S_ISDIR(st_src.st_mode)){
                src_is_reg = 0;
            }
            else{
                printf("[!] src stat error.\n");
                return 3;
            }
            
            // if destination exists, check if it is a regular file or directory
            if(dst_exist){
                if(S_ISREG(st_dst.st_mode)){
                    dst_is_reg = 1;
                }
                else if(S_ISDIR(st_dst.st_mode)){
                    dst_is_reg = 0;
                }
                else{
                    printf("[!] dst stat error.\n");
                    return 3;
                }
            }

            // file - file sitation
            if(src_is_reg && (!dst_exist || dst_is_reg)){
                mode = 1;
            }
            // file - directory situation
            else if(src_is_reg && (dst_exist && !dst_is_reg)){
                mode = 2;  
            }
            // directory directory situation
            else if(!src_is_reg && (dst_exist && !dst_is_reg)){
                mode = 3; 
            }
            // directory - new direcotry situation
            else if(!src_is_reg && !dst_exist){
                mode = 4;  
            }
            else{
                mode = 5;  
            }
            
            // perform normal file copy
            if(mode == 1){
                if(copy(src, dst, root, interactive, verbose, recursively, 0) < 0){
                    printf("[!] Error Copy normal.\n");
                    return 3;
                }
            }
            // perform file into direcotry copy
            else if(mode == 2){
                strcat(dst, "/");
                strcat(dst, src);

                if(copy(src, dst, root, interactive, verbose, recursively, 0) < 0){
                    printf("[!] Error Copy to Directory.\n");
                    return 3;
                }
            }
            // perform directory to directory copy
            else if(mode == 3){
                chdir(dst);
                errno = 0;
                if(mkdir(src, S_IRUSR | S_IWUSR | S_IXUSR) < 0){
                    if(errno != EEXIST){
                        //errors here   
                        printf("[!] Mkdir Error (cp to directory). %d\n", errno);
                        return 3;
                    }
                    
                }
                chdir(root);
                strcat(dst, "/");
                strcat(dst, src);
                explore_dir(src, dst, root, interactive, verbose, recursively, 0);
            }
            // copy directory into new directory
            else if(mode == 4){
                if(mkdir(dst, S_IRUSR | S_IWUSR | S_IXUSR) < 0){
                    printf("[!] Mkdir Error (cp to new directory).\n");
                    return 3;
                }
                explore_dir(src, dst, root, interactive, verbose, recursively, 0);
            }
            else{
                printf("[!] Error: src is directory and dst is regular file.\n");
                return 3;
            }


            free(target_dir);

        }

        return 1;
    }
}

// Function to handle the mv command
int move_function(char **args){
    if(args[1] == NULL){
        printf("[!] Missing operands.\n");return 3;
    }
    else{
        /*
            -t, --target-directory=DIRECTORY
            move all SOURCE arguments into DIRECTORY

            -i, --interactive
            prompt before overwrite (mv: overwrite 'F1/file1'? y)

            -S, --suffix=SUFFIX
            override the usual backup suffixY
        */

        int interactive = 0;
        int suffix = 0;
        int verbose = 0;
        char *suffix_data = (char*)malloc((128) * sizeof(char));
        char* target_dir = (char*)malloc((128) * sizeof(char));
        int target = 0, i, src_is_reg, dst_is_reg = 0, dst_exist = 1, mode;
        struct stat st_src, st_dst;
        int arg_cnt = 1;
        int opt; 
        
        do{
            arg_cnt++;
        }
        while( args[arg_cnt] != NULL );


        while ((opt = getopt(arg_cnt, args, "iS:t:")) != -1) {
            switch(opt)  {  
                case 't':  
                    target = 1;
                    strcpy(target_dir, optarg+1); 
                    break;
                case 'S':  
                    suffix = 1;
                    strcpy(suffix_data, optarg+1); 
                    break;
                case 'i':   
                    interactive = 1;
                    break;  
                case '?':  
                    printf("[!] Unknown option: %c\n", optopt); 
                    break;  
            }  
        } 

        if(target == 0){
            if(optind+2 != arg_cnt){
                printf("[!] Missing operands (source / destination).\n");return 3;
            }
        }

        // ================================ DO ACTUALY MOVE

        char root[DIR_BUF_SIZE];
        getcwd(root, DIR_BUF_SIZE);

        char *dst;
        if(target){
            dst = target_dir;
            // printf(">> %s \n", dst);
        }
        else{
            dst = malloc(strlen(args[arg_cnt-1])+1); // destination
            strcpy(dst, args[arg_cnt-1]);
            dst[strlen(dst)] = '\x00';
        }

        printf("DEST= %s \n", dst);
        char *src = args[optind]; // source

        if(stat(src, &st_src) == -1){ // check if source exists
            printf("[!] src stat failed.");
            return 3;
        }

        if(stat(dst, &st_dst) == -1){ // check if destination exists
            dst_exist = 0;
        }

        // check if source is regular file
        if(S_ISREG(st_src.st_mode)){
            src_is_reg = 1;
        }
        // check if source is directory
        else if(S_ISDIR(st_src.st_mode)){
            src_is_reg = 0;
        }
        else{
            printf("[!] src stat error.\n");
            return 3;
        }
        
        // if destination exists, check if it is a regular file or directory
        if(dst_exist){
            if(S_ISREG(st_dst.st_mode)){
                dst_is_reg = 1;
            }
            else if(S_ISDIR(st_dst.st_mode)){
                dst_is_reg = 0;
            }
            else{
                printf("[!] dst stat error.\n");
                return 3;
            }
        }

        // rename situation
        if(dst_exist == 0 && dst_is_reg == 0){
            puts("aiciiii");
            rename(src, dst);
            if(verbose){
                printf("renamed '%s' -> '%s'\n", src, dst);
            }

            return 1;
        }

        // file - file sitation
        if(src_is_reg && (!dst_exist || dst_is_reg)){
            mode = 1;
        }
        // file - directory situation
        else if(src_is_reg && (dst_exist && !dst_is_reg)){
            mode = 2;  
        }
        // directory directory situation
        else if(!src_is_reg && (dst_exist && !dst_is_reg)){
            mode = 3; 
        }
        // directory - new direcotry situation
        else if(!src_is_reg && !dst_exist){
            mode = 4;  
        }
        else{
            mode = 5;  
        }
        
        // perform normal file copy
        if(mode == 1){
            if(copy(src, dst, root, interactive, 0, 1, suffix) < 0){
                printf("[!] Error Copy normal.\n");
                return 3;
            }
        }
        // perform file into direcotry copy
        else if(mode == 2){
            strcat(dst, "/");
            strcat(dst, src);

            if(copy(src, dst, root, interactive, 0, 1, suffix) < 0){
                printf("[!] Error Copy to Directory.\n");
                return 3;
            }
        }
        // perform directory to directory copy
        else if(mode == 3){
            chdir(dst);
            errno = 0;
            if(mkdir(src, S_IRUSR | S_IWUSR | S_IXUSR) < 0){
                if(errno != EEXIST){
                    //errors here   
                    printf("[!] Mkdir Error (cp to directory). %d\n", errno);
                    return 3;
                }
                
            }
            chdir(root);
            strcat(dst, "/");
            strcat(dst, src);
            explore_dir(src, dst, root, interactive, 0, 1, suffix);
        }
        // copy directory into new directory
        else if(mode == 4){
            if(mkdir(dst, S_IRUSR | S_IWUSR | S_IXUSR) < 0){
                printf("[!] Mkdir Error (cp to new directory).\n");
                return 3;
            }
            explore_dir(src, dst, root, interactive, 0, 1, suffix);
        }
        else{
            printf("[!] Error: src is directory and dst is regular file.\n");
            return 3;
        }

        remove(src);

        free(target_dir);
        return 1;
    }
}

// Function to display basic information about the current shell
int version_info(char **args){
    if (args[1] == NULL) {
        // Print Header Info
        asm("xor rax, rax;");
        asm("push rax");
        asm("mov rbx, 0x0a3d3d3d3d3d3d3d;");
        asm("push rbx;");
        asm("mov rbx, 0x3d206e6f6974616d;");
        asm("push rbx;");
        asm("mov rbx, 0x726f666e49206c6c;");
        asm("push rbx;");
        asm("mov rbx, 0x656853203d3d3d3d;");
        asm("push rbx;");
        asm("mov rbx, 0x0000003d3d3d3d00;");
        asm("push rbx;");
        asm("mov rdi, 0x1;");
        asm("mov rsi, rsp;");
        asm("mov rdx, 0x28;");
        asm("mov rax, 0x1;");
        asm("syscall");

        printf(" [*] Author: Fineas Silaghi (FeDEX) \n");
        printf(" [*] Version: 1.1 \n");

        asm("xor rax, rax;");
        asm("push rax");
        asm("mov rbx, 0x0a3d3d3d3d3d3d3d;");
        asm("push rbx;");
        asm("mov rbx, 0x3d3d3d3d3d3d3d3d;");
        asm("push rbx;");
        asm("mov rbx, 0x3d3d3d3d3d3d3d3d;");
        asm("push rbx;");
        asm("mov rbx, 0x3d3d3d3d3d3d3d3d;");
        asm("push rbx;");
        asm("mov rbx, 0x0000003d3d3d3d00;");
        asm("push rbx;");
        asm("mov rdi, 0x1;");
        asm("mov rsi, rsp;");
        asm("mov rdx, 0x28;");
        asm("mov rax, 0x1;");
        asm("syscall");
    } 
    else{
        printf("[!] Version requires no arguments.\n");return 3;
    }
}

// Function to handle directory creation
int create_directory(char **args){
    int c,arg_cnt = 0;
    mode_t mode = 0777;
    char path[128];
    getcwd(path, sizeof(path));
    while(args[arg_cnt] != NULL){
        arg_cnt++;
    }
    while ((c = getopt(arg_cnt, args, "m:p:")) != -1) {
        switch (c) {
            case 'm':
                mode=(mode_t)strtol(optarg,NULL,8);
                break;
            case 'p':
                strcpy(path,optarg);
                break;
            default:
                printf("[!] Expected -m or -p options only for \"%s\"\n",args[0]);
                return 3;
        }
    }
    arg_cnt -= optind;
    args += optind;
    if (*args) {
            strcat(path,"/");strcat(path,*args);
        if(mkdir(path,mode)==-1){
            printf("[!] Mkdir failed.\n");
            return 3;
        }
    }
    else{
        printf("[!] Expected directory name argument for \"mkdir\"\n");
        return 3;
    }
    return 1;
}

// Function to display help message
int help_function(char **args){
    if (args[1] == NULL) {
        printf("Builtin Commands:\n");
        for (int i = 0; i < (sizeof(custom_commands) / sizeof(char *)); i++) {
            printf(" [%d] %s\n", i,custom_commands[i]);
        }
    } 
    
    return 1;
}

// Function to handle the execution of redirections
int handle_redirection_command(char *user_input, int io_type){
    pid_t pid, wpid;
    // mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int fd, status = 0, pos, arg_cnt = 0;

    char **args = splitline(user_input);
    do{
        arg_cnt++;
    }
    while( args[arg_cnt] != NULL );

    
    for(int i = 0; i < arg_cnt; i++){
        if (strcmp(args[i],">") == 0){
            pos = i;
            break;
        }
        if(strcmp(args[i],"<") == 0){
            pos = i;
            break;
        }
    }


    pid = fork();
    if (pid == 0) {
        // input redirection
        if(io_type == 0){  
            fd = open(args[pos+1], O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }
        // output redirection
        else{       
            fd = open(args[pos+1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        }
        if(fd < 0){
            fprintf(stderr, "[!] Error.\n");
        }
        else {
            dup2(fd, io_type);   
            close(fd);      
            args[pos+1] = NULL;
            args[pos] = NULL;
            if(execvp(args[0], args) == -1) {
                printf("[!] Error.\n");
            }
            return 3;
        }
    }
    else if (pid < 0) { 
        printf("[!] Error.\n");
    } 
    else {
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } 
        while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }        
}

// Function to handle the execution of piped commands
int handle_pipe_command(char *user_input){
	
    int commandc=0,numpipes=0,status;
	pid_t pid;
	char **args;

    // Count total number of pipes in the command
	for (int i = 0; user_input[i]!='\0'; i++){
        if(i>0){
            if(user_input[i] == '|' && user_input[i+1] != '|' && user_input[i-1]!= '|'){
                numpipes++;
            }
        }
    }

	int* pipefds=(int*)malloc((2*numpipes) * sizeof(int));
	char* token=(char*)malloc((128) * sizeof(char));

	token=strtok_r(user_input,"|",&user_input);

    // Create pipes for each command
	for(int i = 0; i < numpipes; i++ ){	
    	if( pipe(pipefds+i*2) < 0 ){
            printf("[!] Pipe creation failed.\n");
            return 3;
        }
	}

    // Run Commands
	do{
	    pid = fork();

         //Child Process
	    if( pid == 0 ){
	        if( commandc != 0 ){
	           	if( dup2(pipefds[(commandc-1)*2], 0) < 0){
                    printf("[!] Child Process could not get Input.\n");
                    exit(1);
                }
        	}
        	if(commandc!=numpipes){
            	if( dup2(pipefds[commandc*2+1], 1) < 0 ){
                    printf("[!] Child Process could not get Output.\n");
                    exit(1);
                }
       	 	}
            for(int i = 0; i < 2*numpipes; i++ ){
                close(pipefds[i]);
            }
	        args=splitline(token);
            execvp(args[0],args);
            printf("[!] Execvp Failed\n");exit(1);
 	    } 

         //Error in Fork
	    else if( pid < 0 ){
            printf("[!] Fork Failed\n");
            return 3;
        }

        //Parent Process
 	    commandc++;
	}
	while(commandc<numpipes+1 && (token=strtok_r(NULL,"|",&user_input)) );
	
    // Close Pipes
    for(int i = 0; i < 2*numpipes; i++ ){
        close(pipefds[i]);
    }
    while (wait(NULL) != -1);

	free(pipefds);
	return 1;
}

// Function to run command
int run_command(char **args){
    pid_t pid;
    int status;
    pid = fork();
    if (pid == 0) { //child process
        if (execvp(args[0], args) == -1) {
            printf("[!] child process: execution error\n");
            return 3; 
        }
        return 3;
    } 
    else if (pid < 0) {
        printf("[!] forking error\n");return 3;
    } 
    else {//parent process
        do {
            waitpid(pid, &status, WUNTRACED);
        } 
        while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    if(status==0)
        return 1;
    return 3;
}

// Function to handle the execution of non-piped commands
int execute(char **args) {
    if (args[0] == NULL) {   
        return 1; 
    }

    // Use user-defined functions for specific commands
    for (int i = 0; i < (sizeof(custom_commands) / sizeof(char *)); i++) {
        if (strcmp(args[0], custom_commands[i]) == 0) {
            switch(i){
                case 0:return change_directory(args);break;	
                case 1:return help_function(args);break;	
                case 2:return exit_function(args);break;	
                case 3:return display_history(args);break;	
                case 4:return create_directory(args);break;
                case 5:return version_info(args);break;
                case 6:return dirname_function(args);break;
                case 7:return copy_function(args);break;
                case 8:return move_function(args);break;
            }
        }
    }

    // Run the command using execvp
    return run_command(args);
}

// Main flow of the program
int main(int argc, char **argv){
    
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    // Local Variables
    int i,cmd_cnt=0,cmd_stat=1;
    char *user_input, *oneline,*tmp,**args,dir_path[256];

    using_history();

    do {
        cmd_cnt=0;

        if(new_path==0){
            getcwd(dir_path,sizeof(dir_path)); // get path
        } 
        
        // Get User Command
        if(argc > 1){
            user_input = malloc(2000);
            memset(user_input, 0, 2000);
            for(i = 1; i < argc; i++){
                strcat(user_input, argv[i]);
                strcat(user_input, " ");
            }
        }
        else{
            // Display Fancy Prompt
            printf("\033[36m\033[40m"); 
            printf(" 🐚 ");
            printf("\033[0m");

            printf("\033[36m\033[47m"); 
            printf(" %s ",dir_path);
            printf("\033[0m");

            printf("\033[37m\033[46m"); 
            printf(" user@local_session ");
            printf("\033[0m");

            printf("\033[36m\033[40m"); 
            printf(" > ");
            printf("\033[0m");
            user_input = readline(ps); // local command
        }
        add_history(user_input);

        // Parse User Input
        for (i = 0; user_input[i] != '\x00'; i++){
            // Check if there are chained commands using ; symbol
            if(user_input[i] == ';'){
                cmd_cnt++;
            }
            // Check if there are pipes used ( avoid the || operator)
            else if(i > 0 && user_input[i] == '|' && user_input[i-1] != '|' && user_input[i+1] != '|'){
                handle_pipe_command(user_input);
                goto piped_command;
            }
            else if(i > 0 && user_input[i] == '>' && user_input[i-1] != '>' && user_input[i+1] != '>'){
                handle_redirection_command(user_input, 1);
                goto piped_command;
            }
            else if(i > 0 && user_input[i] == '<' && user_input[i-1] != '<' && user_input[i+1] != '<'){
                handle_redirection_command(user_input, 0);
                goto piped_command;
            }
        }

        // Handle Non-Piped Command
        tmp = (char*)malloc((strlen(user_input) + 1) * sizeof(char));
        strcpy(tmp,";");
        strcat(tmp,user_input);
        oneline=strtok_r(tmp,";",&tmp);

        // Do-While Loop to execute all commands ( there has to be at least one)
        do{
            if(oneline != NULL){
                args = splitline(oneline); // get arguments
                cmd_stat = execute(args); // execute command
            }
            oneline=strtok_r(NULL,";",&tmp); // get the next command
            cmd_cnt--;
        }
        while(cmd_cnt>-1 && cmd_stat);

        // Command Status 2 means the path was changed
        if(cmd_stat==2){ 
            new_path=1;
            if(args[1]!=NULL){
                strcpy(dir_path,args[1]);
            }
            else{
                strcpy(dir_path,"Command: ");
            }
        }

        free(args);
        piped_command: free(user_input);
        if(argc > 1){
            break;
        }
    } 
    while (cmd_stat);

    // free(tmp);

    return EXIT_SUCCESS;
}