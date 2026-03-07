#include <stdint.h>
#include <winerror.h>

import kernelbase;
import print;

extern "C"
bool CreateProcessW(const char16_t* /*application_name*/, char16_t* /*command_line*/,
                    SECURITY_ATTRIBUTES* /*process_attributes*/,
                    SECURITY_ATTRIBUTES* /*thread_attributes*/,
                    bool /*inherit_handles*/, uint32_t /*creation_flags*/,
                    void* /*environment*/, const char16_t* /*current_directory*/,
                    STARTUPINFOW* /*startup_info*/,
                    PROCESS_INFORMATION* /*process_information*/) {
    print("FIXME - CreateProcessW\n");

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    return false;
}
