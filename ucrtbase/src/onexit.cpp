import ucrtbase;
import print;

extern "C"
int _initialize_onexit_table(_onexit_table_t* /*table*/) {
    print_stderr("FIXME - _initialize_onexit_table\n");

    return 0;
}

extern "C"
int _register_onexit_function(_onexit_table_t* /*table*/, _onexit_t /*func*/) {
    print_stderr("FIXME - _register_onexit_function\n");

    return 0;
}

extern "C"
int _execute_onexit_table(_onexit_table_t* /*table*/) {
    print_stderr("FIXME - _execute_onexit_table\n");

    return 0;
}
