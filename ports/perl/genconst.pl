# Runs a module's own constant generator (its Makefile.PL, or regen.pl for
# IPC::SysV) under miniperl, with MakeMaker's WriteMakefile made a no-op and
# ExtUtils::Constant's output redirected into the given directory. The
# generator is run with the module's source directory as cwd, which is
# where B's Makefile.PL looks for the core headers and regen.pl for
# lib/IPC/SysV.pm, and PERL_CORE is set so Socket's compiler probes are
# skipped; nothing is written there.
#
# usage: miniperl genconst.pl <output dir> <generator script>

use strict;
use warnings;

my ($out, $script) = @ARGV;
die "usage: genconst.pl <output dir> <script>\n" unless defined $script;

BEGIN {
    package ExtUtils::MakeMaker;

    sub WriteMakefile { }
    sub prompt { $_[1] }

    sub import {
        no strict 'refs';
        my $caller = caller;
        *{"${caller}::WriteMakefile"} = \&WriteMakefile;
        *{"${caller}::prompt"} = \&prompt;
    }

    $INC{'ExtUtils/MakeMaker.pm'} = __FILE__;
}

require ExtUtils::Constant;

{
    no warnings 'redefine';
    my $orig = \&ExtUtils::Constant::WriteConstants;
    *ExtUtils::Constant::WriteConstants = sub {
        $orig->(@_, C_FILE => "$out/const-c.inc", XS_FILE => "$out/const-xs.inc");
    };
}

# B's Makefile.PL finds the core headers relative to cwd only when told it
# is a core build; regen.pl treats @ARGV as the list of things to regenerate
$ENV{PERL_CORE} = 1;
@ARGV = $script =~ /Makefile\.PL$/ ? ('PERL_CORE=1') : ();

die "$script: $!\n" unless -r $script;
do "./$script";
die $@ if $@;
