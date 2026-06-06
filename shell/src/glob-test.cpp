#include <stdio.h>
#include <glob.h>

int main() {
    glob_t g;

    if (glob("*.txt", 0, nullptr, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; i++) {
            printf("%s\n", g.gl_pathv[i]);
        }
    }

    globfree(&g);

    return 0;
}
