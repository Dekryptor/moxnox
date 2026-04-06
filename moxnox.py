import gdb

class LoadContext(gdb.Command):
    """Set GDB registers from an NT CONTEXT struct: load_context <expr>"""
    def __init__(self):
        super().__init__("load_context", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        ctx = gdb.parse_and_eval(f"*(CONTEXT*)({arg})")
        regs = [
            ("rax", "Rax"), ("rbx", "Rbx"), ("rcx", "Rcx"), ("rdx", "Rdx"),
            ("rsi", "Rsi"), ("rdi", "Rdi"), ("rbp", "Rbp"), ("rsp", "Rsp"),
            ("r8",  "R8"),  ("r9",  "R9"),  ("r10", "R10"), ("r11", "R11"),
            ("r12", "R12"), ("r13", "R13"), ("r14", "R14"), ("r15", "R15"),
            ("rip", "Rip"), ("eflags", "EFlags"),
        ]
        for gdb_reg, field in regs:
            gdb.execute(f"set ${gdb_reg} = {int(ctx[field]):#x}")

        gdb.execute(f"set $mxcsr = {int(ctx['FltSave']['MxCsr']):#x}")

        for i in range(16):
            xmm = ctx["FltSave"]["XmmRegisters"][i]
            lo = int(xmm["Low"])
            hi = int(xmm["High"])
            gdb.execute(f"set $xmm{i}.v2_int64[0] = {lo:#x}")
            gdb.execute(f"set $xmm{i}.v2_int64[1] = {hi:#x}")

LoadContext()
