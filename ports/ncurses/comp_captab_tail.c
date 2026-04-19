/* *INDENT-ON* */

#define build_names(root) _nc_ ## root ## _table
#define build_alias(root) _nc_ ## root ## alias_table

NCURSES_EXPORT(const struct name_table_entry *)
_nc_get_table(bool termcap)
{
    return termcap ? build_names(cap) : build_names(info);
}

NCURSES_EXPORT(const HashValue *)
_nc_get_hash_table(bool termcap)
{
    return termcap ? _nc_cap_hash_table : _nc_info_hash_table;
}

NCURSES_EXPORT(const struct alias *)
_nc_get_alias_table(bool termcap)
{
    return termcap ? build_alias(cap) : build_alias(info);
}

static HashValue
info_hash(const char *string)
{
    long sum = 0;

    DEBUG(9, ("hashing %s", string));
    while (*string) {
        sum += (long) (UChar(*string) + (UChar(*(string + 1)) << 8));
        string++;
    }

    DEBUG(9, ("sum is %ld", sum));
    return (HashValue) (sum % HASHTABSIZE);
}

#define TCAP_LEN 2

static HashValue
tcap_hash(const char *string)
{
    char temp[TCAP_LEN + 1];
    int limit = 0;

    while (*string) {
        temp[limit++] = *string++;
        if (limit >= TCAP_LEN)
            break;
    }
    temp[limit] = '\0';
    return info_hash(temp);
}

static int
compare_tcap_names(const char *a, const char *b)
{
    return !strncmp(a, b, (size_t) TCAP_LEN);
}

static int
compare_info_names(const char *a, const char *b)
{
    return !strcmp(a, b);
}

static const HashData hash_data[2] =
{
    {HASHTABSIZE, _nc_info_hash_table, info_hash, compare_info_names},
    {HASHTABSIZE, _nc_cap_hash_table, tcap_hash, compare_tcap_names}
};

NCURSES_EXPORT(const HashData *)
_nc_get_hash_info(bool termcap)
{
    return &hash_data[(termcap != FALSE)];
}

#if NO_LEAKS
NCURSES_EXPORT(void)
_nc_comp_captab_leaks(void)
{
}
#endif /* NO_LEAKS */
