# MKhash.awk - generate hash table data for comp_captab.c / comp_userdefs.c
# Replaces the make_hash host-tool step from the ncurses autotools build.
#
# Parameters (via -v):
#   mode      = "info"  -> column 1 (short terminfo names), skip userdefs
#               "cap"   -> column 3 (termcap codes), skip userdefs
#               "user"  -> column 1 (userdef names only), merge duplicate names
#   HASHTABSIZE = numeric value from hashsize.h (e.g. 994)

BEGIN {
    # Build ord() lookup table
    for (i = 0; i < 256; i++)
        _ord[sprintf("%c", i)] = i

    n_entries = 0
    bool_count = 0
    num_count  = 0
    str_count  = 0
}

function ord(c) { return _ord[c] }

function hash_function(str,    sum, i, c1, c2, n) {
    sum = 0
    n = length(str)
    for (i = 1; i <= n; i++) {
        c1 = ord(substr(str, i, 1))
        c2 = (i < n) ? ord(substr(str, i+1, 1)) : 0
        sum += c1 + c2 * 256
    }
    return int(sum % HASHTABSIZE)
}

function get_type_user(mask,    result, sep) {
    result = "("
    sep = ""
    if (int(mask) % 2) { result = result sep "1<<BOOLEAN"; sep = "|" }
    if (int(mask / 2) % 2) { result = result sep "1<<NUMBER"; sep = "|" }
    if (int(mask / 4) % 2) { result = result sep "1<<STRING"; sep = "|" }
    return result ")"
}

/^#/ || /^capalias/ || /^infoalias/ || /^SKIPWARN/ || /^$/ { next }

{
    is_userdef = ($1 == "userdef")

    if (mode == "user" && !is_userdef) next
    if (mode != "user" && is_userdef) next

    name = (mode == "cap") ? $4 : $2

    # skip placeholder entries
    if (name == "-" || name == "") next

    type_str = (mode == "user") ? $3 : $3

    if (mode == "user") {
        # merge duplicate names
        found = -1
        for (j = 0; j < n_entries; j++) {
            if (ent_name[j] == name) { found = j; break }
        }
        if (found < 0) {
            idx = n_entries
            ent_name[idx]  = name
            ent_type[idx]  = 0
            ent_link[idx]  = -1
            ent_argc[idx]  = 0
            ent_args[idx]  = 0
            ent_index[idx] = 0
            n_entries++
        } else {
            idx = found
        }
        if (type_str == "bool")     { ent_type[idx] = int(ent_type[idx]) + 1 }
        else if (type_str == "num") { ent_type[idx] = int(ent_type[idx]) + 2 }
        else if (type_str == "str") {
            ent_type[idx] = int(ent_type[idx]) + 4
            ent_index[idx] = str_count++
            arg_fmt = $4
            if (arg_fmt != "-") {
                ent_argc[idx] = length(arg_fmt)
                for (k = 1; k <= length(arg_fmt); k++) {
                    if (substr(arg_fmt, k, 1) == "s")
                        ent_args[idx] = int(ent_args[idx]) + (2 ^ (k-1))
                }
            }
        }
    } else {
        idx = n_entries
        ent_name[idx] = name
        ent_link[idx] = -1
        if (type_str == "bool") {
            ent_type[idx]  = 0
            ent_index[idx] = bool_count++
        } else if (type_str == "num") {
            ent_type[idx]  = 1
            ent_index[idx] = num_count++
        } else if (type_str == "str") {
            ent_type[idx]  = 2
            ent_index[idx] = str_count++
        } else {
            next
        }
        n_entries++
    }
}

END {
    # Build hash table
    for (i = 0; i < HASHTABSIZE; i++) htab[i] = -1

    collisions = 0
    for (i = 0; i < n_entries; i++) {
        hv = hash_function(ent_name[i])
        if (htab[hv] >= 0) collisions++
        if (htab[hv] != -1) ent_link[i] = htab[hv]
        htab[hv] = i
    }

    root = (mode == "user") ? "user" : (mode == "info") ? "info" : "cap"
    struct_type = (mode == "user") ? "user_table_entry" : "name_table_entry"

    printf "/* %d collisions out of %d entries */\n", collisions, n_entries
    printf "static struct %s const _nc_%s_table[] =\n", struct_type, root
    printf "{\n"
    for (i = 0; i < n_entries; i++) {
        comma = (i < n_entries - 1) ? "," : ""
        if (mode == "user") {
            printf "\t{ %15s,\t%s,\t%d,%d,\t%3d, %3d }%s\n",
                "\"" ent_name[i] "\"",
                get_type_user(ent_type[i]),
                ent_argc[i], ent_args[i],
                ent_index[i], ent_link[i],
                comma
        } else {
            type_name = (ent_type[i] == 0) ? "BOOLEAN" : \
                        (ent_type[i] == 1) ? "NUMBER" : "STRING"
            printf "\t{ %15s,\t%10s,\t%3d, %3d }%s\n",
                "\"" ent_name[i] "\"",
                type_name,
                ent_index[i], ent_link[i],
                comma
        }
    }
    printf "};\n\n"

    printf "static const HashValue _nc_%s_hash_table[%d] =\n", root, HASHTABSIZE + 1
    printf "{\n"
    for (i = 0; i < HASHTABSIZE; i++)
        printf "\t%3d,\n", htab[i]
    printf "\t0\t/* base-of-table */\n"
    printf "};\n\n"

    if (mode == "info") {
        printf "#if (BOOLCOUNT!=%d)||(NUMCOUNT!=%d)||(STRCOUNT!=%d)\n",
            bool_count, num_count, str_count
        printf "#error\t--> term.h and comp_captab.c disagree about the <--\n"
        printf "#error\t--> numbers of booleans, numbers and/or strings <--\n"
        printf "#endif\n\n"
    }
}
