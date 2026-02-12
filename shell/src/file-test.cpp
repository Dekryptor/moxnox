#include <stdio.h>
#include <string.h>

#define GREEN "\e[32m"
#define BOLD_RED "\e[1;31m"
#define NORMAL "\e[0m"

int main() {
    printf("=== File I/O Test ===\n\n");

    const char* filename = "test_file.txt";
    const char* test_data = "Hello, CRT file I/O!\nLine 2\nLine 3\n";

    // Test 1: Write file
    printf("[TEST] fopen/fwrite/fclose\n");
    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("[" BOLD_RED "FAIL" NORMAL "] fopen for write failed\n");
        return 1;
    }

    size_t written = fwrite(test_data, 1, strlen(test_data), f);
    if (written != strlen(test_data)) {
        printf("[" BOLD_RED "FAIL" NORMAL "] fwrite incomplete: %zu/%zu\n", written, strlen(test_data));
        fclose(f);
        return 1;
    }

    fclose(f);
    printf("[" GREEN "PASS" NORMAL "] File written\n");

    // Test 2: Read file
    printf("\n[TEST] fopen/fread\n");
    f = fopen(filename, "r");
    if (!f) {
        printf("[" BOLD_RED "FAIL" NORMAL "] fopen for read failed\n");
        return 1;
    }

    char buffer[256];
    size_t read_bytes = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[read_bytes] = '\0';

    if (strcmp(buffer, test_data) == 0)
        printf("[" GREEN "PASS" NORMAL "] File read correctly\n");
    else {
        printf("[" BOLD_RED "FAIL" NORMAL "] File content mismatch\n");
        printf("  Expected: %s\n", test_data);
        printf("  Got: %s\n", buffer);
    }

    fclose(f);

    // Test 3: fprintf/fscanf
    printf("\n[TEST] fprintf/fscanf\n");
    f = fopen(filename, "w");
    fprintf(f, "%d %s %f\n", 42, "test", 3.14);
    fclose(f);

    f = fopen(filename, "r");
    int num;
    char str[64];
    float flt;
    fscanf(f, "%d %s %f", &num, str, &flt);
    fclose(f);

    if (num == 42 && strcmp(str, "test") == 0 && flt > 3.13 && flt < 3.15)
        printf("[" GREEN "PASS" NORMAL "] fprintf/fscanf work\n");
    else
        printf("[" BOLD_RED "FAIL" NORMAL "] fprintf/fscanf broken: %d %s %f\n", num, str, flt);

    // Test 4: fgets/fputs
    printf("\n[TEST] fgets/fputs\n");
    f = fopen(filename, "w");
    fputs("Line 1\n", f);
    fputs("Line 2\n", f);
    fputs("Line 3\n", f);
    fclose(f);

    f = fopen(filename, "r");
    char line[256];
    int line_count = 0;
    while (fgets(line, sizeof(line), f)) {
        line_count++;
    }
    fclose(f);

    if (line_count == 3)
        printf("[" GREEN "PASS" NORMAL "] fgets/fputs work (%d lines)\n", line_count);
    else
        printf("[" BOLD_RED "FAIL" NORMAL "] Expected 3 lines, got %d\n", line_count);

    // Test 5: fseek/ftell
    printf("\n[TEST] fseek/ftell\n");
    f = fopen(filename, "w");
    fputs("0123456789", f);
    fclose(f);

    f = fopen(filename, "r");
    fseek(f, 5, SEEK_SET);
    long pos = ftell(f);
    int ch = fgetc(f);
    fclose(f);

    if (pos == 5 && ch == '5')
        printf("[" GREEN "PASS" NORMAL "] fseek/ftell work\n");
    else
        printf("[" BOLD_RED "FAIL" NORMAL "] fseek/ftell broken: pos=%ld ch=%c\n", pos, ch);

    // Cleanup
    remove(filename);

    printf("\n=== File I/O tests complete ===\n");
    return 0;
}
