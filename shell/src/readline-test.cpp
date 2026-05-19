#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char* line;

    while ((line = readline("> ")) != nullptr) {
        if (*line)
            add_history(line);

        printf("%s\n", line);
        free(line);
    }

    return 0;
}
