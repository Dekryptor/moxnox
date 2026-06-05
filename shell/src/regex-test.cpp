#include <stdio.h>
#include <string.h>
#include <regex.h>

#define GREEN "\e[32m"
#define BOLD_RED "\e[1;31m"
#define NORMAL "\e[0m"

static int failures;

static void pass(const char* name) {
    printf("[" GREEN "PASS" NORMAL "] %s\n", name);
}

static void fail(const char* name, const char* reason) {
    printf("[" BOLD_RED "FAIL" NORMAL "] %s: %s\n", name, reason);
    failures++;
}

static void check_match(const char* name, const char* pattern, const char* str,
                         int cflags, int eflags, int expect_match) {
    regex_t re;
    int rc = regcomp(&re, pattern, cflags);
    if (rc != 0) {
        char buf[256];
        regerror(rc, &re, buf, sizeof(buf));
        fail(name, buf);
        return;
    }
    rc = regexec(&re, str, 0, nullptr, eflags);
    regfree(&re);
    if (expect_match && rc == REG_NOMATCH)
        fail(name, "expected match, got no match");
    else if (!expect_match && rc == 0)
        fail(name, "expected no match, got match");
    else
        pass(name);
}

static void check_submatch(const char* name, const char* pattern, const char* str,
                            int group, const char* expected) {
    regex_t re;
    int rc = regcomp(&re, pattern, REG_EXTENDED);
    if (rc != 0) {
        char buf[256];
        regerror(rc, &re, buf, sizeof(buf));
        fail(name, buf);
        return;
    }
    regmatch_t m[8] = {};
    rc = regexec(&re, str, 8, m, 0);
    regfree(&re);
    if (rc == REG_NOMATCH) {
        fail(name, "no match");
        return;
    }
    if (m[group].rm_so < 0) {
        fail(name, "group not matched");
        return;
    }
    int len = (int)(m[group].rm_eo - m[group].rm_so);
    if (len != (int)strlen(expected) || strncmp(str + m[group].rm_so, expected, (unsigned)len) != 0) {
        char got[64];
        snprintf(got, sizeof(got), "got '%.*s', want '%s'", len, str + m[group].rm_so, expected);
        fail(name, got);
        return;
    }
    pass(name);
}

int main() {
    printf("=== Regex test ===\n\n");
    failures = 0;

    /* basic ERE match */
    check_match("simple literal match", "hello", "hello world", REG_EXTENDED, 0, 1);
    check_match("simple literal no match","goodbye", "hello world", REG_EXTENDED, 0, 0);
    check_match("anchored match", "^hello$", "hello", REG_EXTENDED, 0, 1);
    check_match("anchored no match", "^hello$", "hello world", REG_EXTENDED, 0, 0);
    check_match("dot wildcard", "h.llo", "hello", REG_EXTENDED, 0, 1);
    check_match("alternation", "cat|dog", "I have a dog", REG_EXTENDED, 0, 1);
    check_match("alternation no match", "cat|dog", "I have a fish", REG_EXTENDED, 0, 0);
    check_match("quantifier plus", "ab+c", "abbc", REG_EXTENDED, 0, 1);
    check_match("quantifier star", "ab*c", "ac", REG_EXTENDED, 0, 1);
    check_match("quantifier question", "colou?r", "color", REG_EXTENDED, 0, 1);
    check_match("quantifier braces", "a{3}", "aaa", REG_EXTENDED, 0, 1);
    check_match("quantifier braces miss", "a{3}", "aa", REG_EXTENDED, 0, 0);

    /* character classes */
    check_match("char class alpha", "[[:alpha:]]+", "hello", REG_EXTENDED, 0, 1);
    check_match("char class digit", "[[:digit:]]+", "12345", REG_EXTENDED, 0, 1);
    check_match("char class digit miss", "[[:digit:]]+", "hello", REG_EXTENDED, 0, 0);
    check_match("char class space", "[[:space:]]", "a b", REG_EXTENDED, 0, 1);
    check_match("bracket expr", "[aeiou]", "rhythm", REG_EXTENDED, 0, 0);
    check_match("negated bracket", "[^aeiou]+", "rhythm", REG_EXTENDED, 0, 1);

    /* case-insensitive */
    check_match("icase match", "HELLO", "hello world", REG_EXTENDED | REG_ICASE, 0, 1);
    check_match("icase no match", "HELLO", "hello world", REG_EXTENDED, 0, 0);

    /* REG_NEWLINE: ^ and $ match at embedded newlines */
    check_match("newline anchor match", "^second$", "first\nsecond\n", REG_EXTENDED | REG_NEWLINE, 0, 1);
    check_match("newline anchor no match","^second$", "first\nsecond\n", REG_EXTENDED, 0, 0);
    /* REG_NEWLINE: dot does not match newline */
    check_match("newline dot miss", "fir.t", "fir\nt", REG_EXTENDED | REG_NEWLINE, 0, 0);

    /* REG_NOTBOL / REG_NOTEOL */
    check_match("notbol", "^hello", "hello", REG_EXTENDED, REG_NOTBOL, 0);
    check_match("noteol", "world$", "world", REG_EXTENDED, REG_NOTEOL, 0);

    /* submatches */
    check_submatch("submatch group 0", "(foo)(bar)", "foobar", 0, "foobar");
    check_submatch("submatch group 1", "(foo)(bar)", "foobar", 1, "foo");
    check_submatch("submatch group 2", "(foo)(bar)", "foobar", 2, "bar");
    check_submatch("nested groups", "(a(b)c)", "abc", 1, "abc");
    check_submatch("nested inner group", "(a(b)c)", "abc", 2, "b");
    check_submatch("alternation submatch","(cat|dog)s", "dogs", 1, "dog");

    /* error handling */
    {
        regex_t re;
        int rc = regcomp(&re, "[invalid", REG_EXTENDED);
        if (rc == 0) {
            regfree(&re);
            fail("bad pattern rejected", "regcomp succeeded on invalid pattern");
        } else {
            char buf[256];
            size_t len = regerror(rc, &re, buf, sizeof(buf));
            if (len > 0 && buf[0] != '\0')
                pass("bad pattern rejected with message");
            else
                fail("bad pattern rejected with message", "regerror returned empty string");
        }
    }

    printf("\n");
    if (failures == 0)
        printf("All tests passed.\n");
    else
        printf("%d test(s) FAILED.\n", failures);

    return failures ? 1 : 0;
}
