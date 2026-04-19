
#define USERTABSIZE SIZEOF(_nc_user_table)

#define build_names(root) _nc_ ## root ## _table

NCURSES_EXPORT(const struct user_table_entry *) _nc_get_userdefs_table (void)
{
    return build_names(user) ;
}

static HashValue
info_hash(const char *string)
{
    long sum = 0;

    DEBUG(9, ("hashing %s", string));
    while (*string) {
        sum += (long) (*string + (*(string + 1) << 8));
        string++;
    }

    DEBUG(9, ("sum is %ld", sum));
    return (HashValue) (sum % HASHTABSIZE);
}

static int
compare_info_names(const char *a, const char *b)
{
    return !strcmp(a, b);
}

static const HashData hash_data[] = {
    { HASHTABSIZE, _nc_user_hash_table, info_hash, compare_info_names }
};

NCURSES_EXPORT(const HashData *) _nc_get_hash_user (void)
{
    return hash_data;
}

#if NO_LEAKS
NCURSES_EXPORT(void) _nc_comp_userdefs_leaks(void)
{
}
#endif /* NO_LEAKS */

#else /*! NCURSES_XNAMES */
NCURSES_EXPORT(void) _nc_comp_userdefs(void);
NCURSES_EXPORT(void) _nc_comp_userdefs(void) { }
#endif /* NCURSES_XNAMES */
