#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>



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