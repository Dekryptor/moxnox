// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>

int main(void) {
    FILE* f = popen("ls", "r");

    if (!f) {
        perror("popen");
        return 1;
    }

    char line[1024];

    while (fgets(line, sizeof(line), f)) {
        fputs(line, stdout);
    }

    int ret = pclose(f);

    printf("pclose returned %d\n", ret);

    return 0;
}
