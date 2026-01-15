module;

#include <string_view>

export module process;

import object_ref;

using namespace std;

export object_ref create_process(string_view fn, string_view curdir,
                                 u16string_view cmdline, object_ref& stdinobj,
                                 object_ref& stdoutobj, object_ref& stderrobj);
export void load_ntdll();
export void create_time_page();
