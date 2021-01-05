gcc shell.c -lreadline -masm=intel -lpthread -o shell
gcc server.c -lpthread -o server
gcc client.c -o client