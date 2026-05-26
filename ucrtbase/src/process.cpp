import nt;

extern "C"
int _getpid() {
    PROCESS_BASIC_INFORMATION pbi;

    auto status = NtQueryInformationProcess(NtCurrentProcess(),
                                            PROCESSINFOCLASS::BasicInformation,
                                            &pbi, sizeof(pbi), nullptr);
    if (!success(status))
        return 0;

    return (int)pbi.UniqueProcessId;
}
