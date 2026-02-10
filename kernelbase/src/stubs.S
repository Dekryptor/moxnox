.macro do_stub name
    .text
    .global "\name"
    "\name":
    ud2

    .section .drectve
    .ascii " -export:\"\name\""
.endm

do_stub "GetLastError"
do_stub "SetUnhandledExceptionFilter"
do_stub "TlsGetValue"
do_stub "VirtualProtect"
do_stub "VirtualQuery"
