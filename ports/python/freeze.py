#!/usr/bin/env python3
"""Generate CPython frozen-module headers by driving Programs/_freeze_module.

The marshalled bytecode in the frozen .h files has to match the 3.13 interpreter
being built, so the compilation is done by _freeze_module - a tool built from
this exact CPython tree (see the python_freeze_host ExternalProject) - rather than
by this interpreter. That means THIS script only needs to parse the module list
and shell out, so it can run under any host Python version (the build machine's
3.14 is fine), which is the whole point: the version-locked work lives in the
compiled tool, not here.

The module list (and the source/alias/glob handling) is taken straight from the
repo's own Tools/build/freeze_modules.py, so this stays in step with upstream.
getpath.py is frozen separately, exactly as Makefile.pre.in does it.
"""

import os
import subprocess
import sys


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: freeze.py <cpython-repo> <out-dir> <_freeze_module>")
    repo_dir, out_dir, freeze_tool = sys.argv[1], sys.argv[2], sys.argv[3]

    sys.path.insert(0, os.path.join(repo_dir, "Tools", "build"))
    import freeze_modules as fm

    outdir = os.path.join(out_dir, "Python", "frozen_modules")
    os.makedirs(outdir, exist_ok=True)

    def freeze_one(frozenid, pyfile, outpath):
        # _freeze_module <frozenid> <input.py> <output.h>, matching how
        # Makefile.pre.in / regen-frozen invoke it.
        subprocess.run([freeze_tool, frozenid, pyfile, outpath], check=True)

    for src in fm._iter_sources(list(fm.parse_frozen_specs())):
        freeze_one(src.frozenid, src.pyfile,
                   os.path.join(outdir, os.path.basename(src.frozenfile)))

    # getpath.py is frozen separately from freeze_modules (see Makefile.pre.in).
    freeze_one("getpath", os.path.join(repo_dir, "Modules", "getpath.py"),
               os.path.join(outdir, "getpath.h"))


if __name__ == "__main__":
    main()
