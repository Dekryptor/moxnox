module;

#include <termios.h>

export module termios;

export using ::termios;

export using ::tcflag_t;
export using ::cc_t;
export using ::speed_t;

/* c_iflag */
constexpr tcflag_t _IGNBRK = IGNBRK;
constexpr tcflag_t _BRKINT = BRKINT;
constexpr tcflag_t _IGNPAR = IGNPAR;
constexpr tcflag_t _PARMRK = PARMRK;
constexpr tcflag_t _INPCK = INPCK;
constexpr tcflag_t _ISTRIP = ISTRIP;
constexpr tcflag_t _INLCR = INLCR;
constexpr tcflag_t _IGNCR = IGNCR;
constexpr tcflag_t _ICRNL = ICRNL;
constexpr tcflag_t _IUCLC = IUCLC;
constexpr tcflag_t _IXON = IXON;
constexpr tcflag_t _IXANY = IXANY;
constexpr tcflag_t _IXOFF = IXOFF;
constexpr tcflag_t _IMAXBEL = IMAXBEL;
constexpr tcflag_t _IUTF8 = IUTF8;
#undef IGNBRK
#undef BRKINT
#undef IGNPAR
#undef PARMRK
#undef INPCK
#undef ISTRIP
#undef INLCR
#undef IGNCR
#undef ICRNL
#undef IUCLC
#undef IXON
#undef IXANY
#undef IXOFF
#undef IMAXBEL
#undef IUTF8
export constexpr tcflag_t IGNBRK = _IGNBRK;
export constexpr tcflag_t BRKINT = _BRKINT;
export constexpr tcflag_t IGNPAR = _IGNPAR;
export constexpr tcflag_t PARMRK = _PARMRK;
export constexpr tcflag_t INPCK = _INPCK;
export constexpr tcflag_t ISTRIP = _ISTRIP;
export constexpr tcflag_t INLCR = _INLCR;
export constexpr tcflag_t IGNCR = _IGNCR;
export constexpr tcflag_t ICRNL = _ICRNL;
export constexpr tcflag_t IUCLC = _IUCLC;
export constexpr tcflag_t IXON = _IXON;
export constexpr tcflag_t IXANY = _IXANY;
export constexpr tcflag_t IXOFF = _IXOFF;
export constexpr tcflag_t IMAXBEL = _IMAXBEL;
export constexpr tcflag_t IUTF8 = _IUTF8;

/* c_oflag */
constexpr tcflag_t _OPOST = OPOST;
constexpr tcflag_t _OLCUC = OLCUC;
constexpr tcflag_t _ONLCR = ONLCR;
constexpr tcflag_t _OCRNL = OCRNL;
constexpr tcflag_t _ONOCR = ONOCR;
constexpr tcflag_t _ONLRET = ONLRET;
constexpr tcflag_t _OFILL = OFILL;
constexpr tcflag_t _OFDEL = OFDEL;
constexpr tcflag_t _NLDLY = NLDLY;
constexpr tcflag_t _NL0 = NL0;
constexpr tcflag_t _NL1 = NL1;
constexpr tcflag_t _CRDLY = CRDLY;
constexpr tcflag_t _CR0 = CR0;
constexpr tcflag_t _CR1 = CR1;
constexpr tcflag_t _CR2 = CR2;
constexpr tcflag_t _CR3 = CR3;
constexpr tcflag_t _TABDLY = TABDLY;
constexpr tcflag_t _TAB0 = TAB0;
constexpr tcflag_t _TAB1 = TAB1;
constexpr tcflag_t _TAB2 = TAB2;
constexpr tcflag_t _TAB3 = TAB3;
constexpr tcflag_t _BSDLY = BSDLY;
constexpr tcflag_t _BS0 = BS0;
constexpr tcflag_t _BS1 = BS1;
constexpr tcflag_t _VTDLY = VTDLY;
constexpr tcflag_t _VT0 = VT0;
constexpr tcflag_t _VT1 = VT1;
constexpr tcflag_t _FFDLY = FFDLY;
constexpr tcflag_t _FF0 = FF0;
constexpr tcflag_t _FF1 = FF1;
#undef OPOST
#undef OLCUC
#undef ONLCR
#undef OCRNL
#undef ONOCR
#undef ONLRET
#undef OFILL
#undef OFDEL
#undef NLDLY
#undef NL0
#undef NL1
#undef CRDLY
#undef CR0
#undef CR1
#undef CR2
#undef CR3
#undef TABDLY
#undef TAB0
#undef TAB1
#undef TAB2
#undef TAB3
#undef BSDLY
#undef BS0
#undef BS1
#undef VTDLY
#undef VT0
#undef VT1
#undef FFDLY
#undef FF0
#undef FF1
export constexpr tcflag_t OPOST = _OPOST;
export constexpr tcflag_t OLCUC = _OLCUC;
export constexpr tcflag_t ONLCR = _ONLCR;
export constexpr tcflag_t OCRNL = _OCRNL;
export constexpr tcflag_t ONOCR = _ONOCR;
export constexpr tcflag_t ONLRET = _ONLRET;
export constexpr tcflag_t OFILL = _OFILL;
export constexpr tcflag_t OFDEL = _OFDEL;
export constexpr tcflag_t NLDLY = _NLDLY;
export constexpr tcflag_t NL0 = _NL0;
export constexpr tcflag_t NL1 = _NL1;
export constexpr tcflag_t CRDLY = _CRDLY;
export constexpr tcflag_t CR0 = _CR0;
export constexpr tcflag_t CR1 = _CR1;
export constexpr tcflag_t CR2 = _CR2;
export constexpr tcflag_t CR3 = _CR3;
export constexpr tcflag_t TABDLY = _TABDLY;
export constexpr tcflag_t TAB0 = _TAB0;
export constexpr tcflag_t TAB1 = _TAB1;
export constexpr tcflag_t TAB2 = _TAB2;
export constexpr tcflag_t TAB3 = _TAB3;
export constexpr tcflag_t BSDLY = _BSDLY;
export constexpr tcflag_t BS0 = _BS0;
export constexpr tcflag_t BS1 = _BS1;
export constexpr tcflag_t VTDLY = _VTDLY;
export constexpr tcflag_t VT0 = _VT0;
export constexpr tcflag_t VT1 = _VT1;
export constexpr tcflag_t FFDLY = _FFDLY;
export constexpr tcflag_t FF0 = _FF0;
export constexpr tcflag_t FF1 = _FF1;

/* c_cflag */
constexpr tcflag_t _CSIZE = CSIZE;
constexpr tcflag_t _CS5 = CS5;
constexpr tcflag_t _CS6 = CS6;
constexpr tcflag_t _CS7 = CS7;
constexpr tcflag_t _CS8 = CS8;
constexpr tcflag_t _CSTOPB = CSTOPB;
constexpr tcflag_t _CREAD = CREAD;
constexpr tcflag_t _PARENB = PARENB;
constexpr tcflag_t _PARODD = PARODD;
constexpr tcflag_t _HUPCL = HUPCL;
constexpr tcflag_t _CLOCAL = CLOCAL;
constexpr tcflag_t _CRTSCTS = CRTSCTS;
#undef CSIZE
#undef CS5
#undef CS6
#undef CS7
#undef CS8
#undef CSTOPB
#undef CREAD
#undef PARENB
#undef PARODD
#undef HUPCL
#undef CLOCAL
#undef CRTSCTS
export constexpr tcflag_t CSIZE = _CSIZE;
export constexpr tcflag_t CS5 = _CS5;
export constexpr tcflag_t CS6 = _CS6;
export constexpr tcflag_t CS7 = _CS7;
export constexpr tcflag_t CS8 = _CS8;
export constexpr tcflag_t CSTOPB = _CSTOPB;
export constexpr tcflag_t CREAD = _CREAD;
export constexpr tcflag_t PARENB = _PARENB;
export constexpr tcflag_t PARODD = _PARODD;
export constexpr tcflag_t HUPCL = _HUPCL;
export constexpr tcflag_t CLOCAL = _CLOCAL;
export constexpr tcflag_t CRTSCTS = _CRTSCTS;

/* c_lflag */
constexpr tcflag_t _ISIG = ISIG;
constexpr tcflag_t _ICANON = ICANON;
constexpr tcflag_t _XCASE = XCASE;
constexpr tcflag_t _ECHO = ECHO;
constexpr tcflag_t _ECHOE = ECHOE;
constexpr tcflag_t _ECHOK = ECHOK;
constexpr tcflag_t _ECHONL = ECHONL;
constexpr tcflag_t _NOFLSH = NOFLSH;
constexpr tcflag_t _TOSTOP = TOSTOP;
constexpr tcflag_t _ECHOCTL = ECHOCTL;
constexpr tcflag_t _ECHOPRT = ECHOPRT;
constexpr tcflag_t _ECHOKE = ECHOKE;
constexpr tcflag_t _FLUSHO = FLUSHO;
constexpr tcflag_t _PENDIN = PENDIN;
constexpr tcflag_t _IEXTEN = IEXTEN;
#undef ISIG
#undef ICANON
#undef XCASE
#undef ECHO
#undef ECHOE
#undef ECHOK
#undef ECHONL
#undef NOFLSH
#undef TOSTOP
#undef ECHOCTL
#undef ECHOPRT
#undef ECHOKE
#undef FLUSHO
#undef PENDIN
#undef IEXTEN
export constexpr tcflag_t ISIG = _ISIG;
export constexpr tcflag_t ICANON = _ICANON;
export constexpr tcflag_t XCASE = _XCASE;
export constexpr tcflag_t ECHO = _ECHO;
export constexpr tcflag_t ECHOE = _ECHOE;
export constexpr tcflag_t ECHOK = _ECHOK;
export constexpr tcflag_t ECHONL = _ECHONL;
export constexpr tcflag_t NOFLSH = _NOFLSH;
export constexpr tcflag_t TOSTOP = _TOSTOP;
export constexpr tcflag_t ECHOCTL = _ECHOCTL;
export constexpr tcflag_t ECHOPRT = _ECHOPRT;
export constexpr tcflag_t ECHOKE = _ECHOKE;
export constexpr tcflag_t FLUSHO = _FLUSHO;
export constexpr tcflag_t PENDIN = _PENDIN;
export constexpr tcflag_t IEXTEN = _IEXTEN;

/* c_cc indices */
constexpr size_t _VINTR = VINTR;
constexpr size_t _VQUIT = VQUIT;
constexpr size_t _VERASE = VERASE;
constexpr size_t _VKILL = VKILL;
constexpr size_t _VEOF = VEOF;
constexpr size_t _VTIME = VTIME;
constexpr size_t _VMIN = VMIN;
constexpr size_t _VSWTC = VSWTC;
constexpr size_t _VSTART = VSTART;
constexpr size_t _VSTOP = VSTOP;
constexpr size_t _VSUSP = VSUSP;
constexpr size_t _VEOL = VEOL;
constexpr size_t _VREPRINT = VREPRINT;
constexpr size_t _VDISCARD = VDISCARD;
constexpr size_t _VWERASE = VWERASE;
constexpr size_t _VLNEXT = VLNEXT;
constexpr size_t _VEOL2 = VEOL2;
#undef VINTR
#undef VQUIT
#undef VERASE
#undef VKILL
#undef VEOF
#undef VTIME
#undef VMIN
#undef VSWTC
#undef VSTART
#undef VSTOP
#undef VSUSP
#undef VEOL
#undef VREPRINT
#undef VDISCARD
#undef VWERASE
#undef VLNEXT
#undef VEOL2
export constexpr size_t VINTR = _VINTR;
export constexpr size_t VQUIT = _VQUIT;
export constexpr size_t VERASE = _VERASE;
export constexpr size_t VKILL = _VKILL;
export constexpr size_t VEOF = _VEOF;
export constexpr size_t VTIME = _VTIME;
export constexpr size_t VMIN = _VMIN;
export constexpr size_t VSWTC = _VSWTC;
export constexpr size_t VSTART = _VSTART;
export constexpr size_t VSTOP = _VSTOP;
export constexpr size_t VSUSP = _VSUSP;
export constexpr size_t VEOL = _VEOL;
export constexpr size_t VREPRINT = _VREPRINT;
export constexpr size_t VDISCARD = _VDISCARD;
export constexpr size_t VWERASE = _VWERASE;
export constexpr size_t VLNEXT = _VLNEXT;
export constexpr size_t VEOL2 = _VEOL2;
