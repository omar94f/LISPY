#include <stdio.h>
#include <stdlib.h>

// __APPLE__ for mac
#ifdef _WIN32

#include <string.h>

static char buffer[2048];

char* readline(char *prompt){
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);

    char *cpy = malloc(strlen(buffer)+1);
    strcpy(cyp, buffer);
    coy[strlen(cpy)-1] = '\0';

    return cpy;
}

void add_history(char *unused) {}

#else

#include <editline/readline.h>

#endif


int main(int arc, char** argv) {
    fputs("Lispy Version 0.0.0.0.1\n", stdout);
    fputs("Press Ctrl+c to Exit\n", stdout);

    while(1){

        char *input = readline("Lispy> ");

        add_history(input);

        printf("No you're a %s\n", input);

        free(input);
    }
    

    return 0;
}