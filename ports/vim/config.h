#define HAVE_CONFIG_H 1
#define UNIX 1

/* Feature level - huge, minus configure-only features (no libvterm, no X11, etc.) */
#define FEAT_HUGE 1

/* Sizes */
#define VIM_SIZEOF_INT 4
#define VIM_SIZEOF_LONG 4
#define SIZEOF_OFF_T 8
#define SIZEOF_TIME_T 8

/* Terminal via ncurses/terminfo */
#define TERMINFO 1
#define HAVE_TERMIOS_H 1
#define HAVE_TGETENT 1
#define HAVE_TCGETWINSIZE 1

/* Use system() for shell commands rather than fork/exec */
#define USE_SYSTEM 1

/* Compiler attributes */
#define HAVE_ATTRIBUTE_UNUSED 1
#define HAVE_DATE_TIME 1

/* Standard headers available */
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STDARG_H 1
#define HAVE_DIRENT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_FCNTL_H 1
#define HAVE_TERMCAP_H 1

/* Use select() rather than poll() for I/O multiplexing */
#define HAVE_SELECT 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_TIME_H 1
#define SYS_SELECT_WITH_SYS_TIME 1
#define SELECT_TYPE_ARG234 (fd_set *)

/* Functions available via ucrtbase */
#define HAVE_STRERROR 1
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_STRTOL 1
#define HAVE_MEMSET 1
#define HAVE_GETCWD 1
#define HAVE_GETENV 1
/* #undef HAVE_PUTENV */
#define HAVE_RENAME 1
#define HAVE_SETENV 1
#define HAVE_STRFTIME 1
#define HAVE_ISINF 1
#define HAVE_ISNAN 1
#define HAVE_FTRUNCATE 1

/* Runtime paths */
#define RUNTIME_GLOBAL "/share/vim/vim91"
#define RUNTIME_GLOBAL_AFTER "/share/vim/vim91/after"
#define SYSCONFDIR "/etc"
