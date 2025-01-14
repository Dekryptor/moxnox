#include "efi.h"

using namespace std;

EFI_SYSTEM_TABLE* systable;
EFI_BOOT_SERVICES* bs;

static const EFI_GUID moxnox_partition_guid = { 0x5dd71063, 0x95c9, 0x41b2, { 0x82, 0x52, 0x84, 0x13, 0xfe, 0xac, 0x64, 0x90 } };

static loader_block lb;

#define KERNEL_STACK_PAGES 4

void print(const char16_t* s) {
    systable->ConOut->OutputString(systable->ConOut, (CHAR16*)s);
}

static span<char16_t> sp_wcscpy(span<char16_t> dest, u16string_view src) {
    auto to_copy = min(src.size(), dest.size());

    memcpy(dest.data(), src.data(), to_copy * sizeof(char16_t));

    return dest.subspan(to_copy);
}

const u16string_view error_string(EFI_STATUS status) {
    switch (status) {
        case EFI_SUCCESS:
            return u"EFI_SUCCESS";

        case EFI_LOAD_ERROR:
            return u"EFI_LOAD_ERROR";

        case EFI_INVALID_PARAMETER:
            return u"EFI_INVALID_PARAMETER";

        case EFI_UNSUPPORTED:
            return u"EFI_UNSUPPORTED";

        case EFI_BAD_BUFFER_SIZE:
            return u"EFI_BAD_BUFFER_SIZE";

        case EFI_BUFFER_TOO_SMALL:
            return u"EFI_BUFFER_TOO_SMALL";

        case EFI_NOT_READY:
            return u"EFI_NOT_READY";

        case EFI_DEVICE_ERROR:
            return u"EFI_DEVICE_ERROR";

        case EFI_WRITE_PROTECTED:
            return u"EFI_WRITE_PROTECTED";

        case EFI_OUT_OF_RESOURCES:
            return u"EFI_OUT_OF_RESOURCES";

        case EFI_VOLUME_CORRUPTED:
            return u"EFI_VOLUME_CORRUPTED";

        case EFI_VOLUME_FULL:
            return u"EFI_VOLUME_FULL";

        case EFI_NO_MEDIA:
            return u"EFI_NO_MEDIA";

        case EFI_MEDIA_CHANGED:
            return u"EFI_MEDIA_CHANGED";

        case EFI_NOT_FOUND:
            return u"EFI_NOT_FOUND";

        case EFI_ACCESS_DENIED:
            return u"EFI_ACCESS_DENIED";

        case EFI_NO_RESPONSE:
            return u"EFI_NO_RESPONSE";

        case EFI_NO_MAPPING:
            return u"EFI_NO_MAPPING";

        case EFI_TIMEOUT:
            return u"EFI_TIMEOUT";

        case EFI_NOT_STARTED:
            return u"EFI_NOT_STARTED";

        case EFI_ALREADY_STARTED:
            return u"EFI_ALREADY_STARTED";

        case EFI_ABORTED:
            return u"EFI_ABORTED";

        case EFI_ICMP_ERROR:
            return u"EFI_ICMP_ERROR";

        case EFI_TFTP_ERROR:
            return u"EFI_TFTP_ERROR";

        case EFI_PROTOCOL_ERROR:
            return u"EFI_PROTOCOL_ERROR";

        case EFI_INCOMPATIBLE_VERSION:
            return u"EFI_INCOMPATIBLE_VERSION";

        case EFI_SECURITY_VIOLATION:
            return u"EFI_SECURITY_VIOLATION";

        case EFI_CRC_ERROR:
            return u"EFI_CRC_ERROR";

        case EFI_END_OF_MEDIA:
            return u"EFI_END_OF_MEDIA";

        case EFI_END_OF_FILE:
            return u"EFI_END_OF_FILE";

        case EFI_INVALID_LANGUAGE:
            return u"EFI_INVALID_LANGUAGE";

        case EFI_COMPROMISED_DATA:
            return u"EFI_COMPROMISED_DATA";

        default:
            return u"(unknown error)";
    }
}

void print_error(u16string_view func, EFI_STATUS status) {
    char16_t s[255];
    auto sp = span(s, (sizeof(s) / sizeof(char16_t)) - 1);

    sp = sp_wcscpy(sp, func);
    sp = sp_wcscpy(sp, u" returned ");
    sp = sp_wcscpy(sp, error_string(status));
    sp = sp_wcscpy(sp, u"\r\n");

    *sp.data() = 0;
    print(s);
}

static EFI_STATUS create_file_device_path(EFI_BOOT_SERVICES* bs, EFI_DEVICE_PATH* fs, u16string_view path,
                                          EFI_DEVICE_PATH_PROTOCOL** pdp) {
    EFI_STATUS status;
    size_t path_len, fslen, fplen;
    FILEPATH_DEVICE_PATH* fp;
    EFI_DEVICE_PATH_PROTOCOL* dp;
    EFI_DEVICE_PATH_PROTOCOL* end_dp;

    path_len = path.size() * sizeof(WCHAR);
    fplen = offsetof(FILEPATH_DEVICE_PATH, PathName[0]) + path_len + sizeof(WCHAR);

    {
        EFI_DEVICE_PATH_PROTOCOL* dpbit = fs;

        fslen = 0;
        do {
            if (dpbit->Type == END_DEVICE_PATH_TYPE)
                break;

            fslen += *(uint16_t*)dpbit->Length;
            dpbit = (EFI_DEVICE_PATH_PROTOCOL*)((uint8_t*)dpbit + *(uint16_t*)dpbit->Length);
        } while (true);
    }

    status = bs->AllocatePool(EfiLoaderData, fslen + fplen + sizeof(EFI_DEVICE_PATH_PROTOCOL), (void**)&dp);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePool", status);
        return status;
    }

    memcpy(dp, fs, fslen);

    fp = (FILEPATH_DEVICE_PATH*)((uint8_t*)dp + fslen);

    fp->Header.Type = MEDIA_DEVICE_PATH;
    fp->Header.SubType = MEDIA_FILEPATH_DP;
    *(uint16_t*)fp->Header.Length = (uint16_t)fplen;

    memcpy(fp->PathName, path.data(), path_len + sizeof(WCHAR));

    end_dp = (EFI_DEVICE_PATH_PROTOCOL*)&fp->PathName[(path_len / sizeof(WCHAR)) + 1];
    SetDevicePathEndNode(end_dp);

    *pdp = dp;

    return EFI_SUCCESS;
}

static EFI_STATUS open_parent_dir(EFI_FILE_IO_INTERFACE* fs, FILEPATH_DEVICE_PATH* dp, EFI_FILE_HANDLE* dir) {
    EFI_STATUS status;
    unsigned int len;
    WCHAR* name;
    EFI_FILE_HANDLE root;

    if (dp->Header.Type != MEDIA_DEVICE_PATH || dp->Header.SubType != MEDIA_FILEPATH_DP)
        return EFI_INVALID_PARAMETER;

    len = *(uint16_t*)dp->Header.Length / sizeof(CHAR16);

    if (len == 0)
        return EFI_INVALID_PARAMETER;

    for (int i = len - 1; i >= 0; i--) {
        if (dp->PathName[i] == '\\') {
            len = i;
            break;
        }
    }

    if (len == 0) {
        if (dp->PathName[0] == '\\')
            len = 1;
        else
            return EFI_INVALID_PARAMETER;
    }

    status = systable->BootServices->AllocatePool(EfiLoaderData, (len + 1) * sizeof(WCHAR), (void**)&name);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePool", status);
        return status;
    }

    memcpy(name, dp->PathName, len * sizeof(WCHAR));
    name[len] = 0;

    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) {
        print_error(u"OpenVolume", status);
        systable->BootServices->FreePool(name);
        return status;
    }

    status = root->Open(root, dir, name, EFI_FILE_MODE_READ, 0);

    systable->BootServices->FreePool(name);
    root->Close(root);

    return status;
}

static EFI_STATUS load_efi_drivers(EFI_HANDLE image_handle) {
    EFI_STATUS status;
    EFI_GUID guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID guid2 = SIMPLE_FILE_SYSTEM_PROTOCOL;
    EFI_GUID guid3 = EFI_DEVICE_PATH_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL* image;
    EFI_FILE_IO_INTERFACE* fs;
    EFI_FILE_HANDLE dir, drivers;
    EFI_DEVICE_PATH* device_path;
    bool drivers_loaded = false;
    char buf[1024];

    static const char16_t drivers_dir[] = u"drivers";

    status = bs->OpenProtocol(image_handle, &guid, (void**)&image, image_handle, NULL,
                              EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status)) {
        print_error(u"OpenProtocol", status);
        return status;
    }

    if (!image->DeviceHandle)
        goto end2;

    status = bs->OpenProtocol(image->DeviceHandle, &guid2, (void**)&fs, image_handle, NULL,
                              EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status)) {
        print_error(u"OpenProtocol", status);
        goto end2;
    }

    status = open_parent_dir(fs, (FILEPATH_DEVICE_PATH*)image->FilePath, &dir);
    if (EFI_ERROR(status)) {
        print_error(u"open_parent_dir", status);
        goto end;
    }

    status = bs->HandleProtocol(image->DeviceHandle, &guid3, (void**)&device_path);
    if (EFI_ERROR(status)) {
        print_error(u"HandleProtocol", status);
        goto end;
    }

    status = dir->Open(dir, &drivers, (WCHAR*)drivers_dir, EFI_FILE_MODE_READ, 0);

    dir->Close(dir);

    if (status == EFI_NOT_FOUND) {
        // drivers directory not found - return without error
        status = EFI_SUCCESS;
        goto end;
    } else if (EFI_ERROR(status)) {
        print(u"Error opening \"drivers\" directory.\r\n");
        print_error(u"Open", status);
        goto end;
    }

    do {
        UINTN size;
        EFI_HANDLE h;
        EFI_DEVICE_PATH_PROTOCOL* dp;
        char16_t path[260];

        size = sizeof(buf);

        status = drivers->Read(drivers, &size, buf);
        if (EFI_ERROR(status)) {
            print_error(u"Read", status);
            drivers->Close(drivers);
            goto end;
        }

        if (size == 0)
            break;

        auto fn = u16string_view((char16_t*)((EFI_FILE_INFO*)buf)->FileName);

        // skip if not .efi file
        if (fn.size() < 4 || (fn[fn.size() - 1] != 'i' && fn[fn.size() - 1] != 'I') || (fn[fn.size() - 2] != 'f' && fn[fn.size() - 2] != 'F') ||
            (fn[fn.size() - 3] != 'e' && fn[fn.size() - 3] != 'E') || fn[fn.size() - 4] != '.')
            continue;

        {
            char16_t s[255];
            auto sp = span(s, (sizeof(s) / sizeof(char16_t)) - 1);

            sp = sp_wcscpy(sp, u"Loading driver ");
            sp = sp_wcscpy(sp, fn);
            sp = sp_wcscpy(sp, u"... ");

            *sp.data() = 0;

            print(s);
        }

        auto path_name = u16string_view((char16_t*)((FILEPATH_DEVICE_PATH*)image->FilePath)->PathName, (*(uint16_t*)image->FilePath->Length) / sizeof(char16_t));
        auto sp = span(path, (sizeof(path) / sizeof(char16_t)) - 1);

        auto last_bs = path_name.find_last_of(u'\\');

        if (last_bs != u16string_view::npos)
            path_name = u16string_view(path_name.data(), last_bs + 1);

        sp = sp_wcscpy(sp, path_name);
        sp = sp_wcscpy(sp, drivers_dir);
        sp = sp_wcscpy(sp, u"\\");
        sp = sp_wcscpy(sp, fn);

        *sp.data() = 0;

        status = create_file_device_path(bs, device_path, path, &dp);
        if (EFI_ERROR(status)) {
            print(u"FAILED\r\n");
            print_error(u"create_file_device_path", status);
            bs->FreePool(dp);
            continue;
        }

        status = bs->LoadImage(false, image_handle, dp, NULL, 0, &h);
        if (EFI_ERROR(status)) {
            print(u"FAILED\r\n");
            print_error(u"LoadImage", status);
            bs->FreePool(dp);
            continue;
        }

        bs->FreePool(dp);

        status = bs->StartImage(h, NULL, NULL);
        if (EFI_ERROR(status)) {
            print(u"FAILED\r\n");
            print_error(u"StartImage", status);
            continue;
        }

        print(u"success\r\n");
        drivers_loaded = true;
    } while (true);

    drivers->Close(drivers);

    if (drivers_loaded) {
        UINTN count;
        EFI_HANDLE* handles;

        status = bs->LocateHandleBuffer(AllHandles, NULL, NULL, &count, &handles);
        if (EFI_ERROR(status)) {
            print_error(u"LocateHandleBuffer", status);
            goto end;
        }

        for (unsigned int i = 0; i < count; i++) {
            bs->ConnectController(handles[i], NULL, NULL, true);
        }

        bs->FreePool(handles);
    }

    status = EFI_SUCCESS;

end:
    bs->CloseProtocol(image->DeviceHandle, &guid2, image_handle, NULL);

end2:
    bs->CloseProtocol(image_handle, &guid, image_handle, NULL);

    return status;
}

static bool try_disk(EFI_BLOCK_IO_PROTOCOL* biop, EFI_GUID* sig) {
    EFI_STATUS status;
    char first_sect[4096];

    static const uint8_t gpt_protective = 0xee;
    static const uint64_t gpt_signature = 0x5452415020494645; // "EFI PART"
    static const unsigned int read_sector_size = 4096;

    status = biop->ReadBlocks(biop, biop->Media->MediaId, 0, sizeof(first_sect), first_sect);
    if (EFI_ERROR(status)) {
        print_error(u"ReadBlocks", status);
        return false;
    }

    auto& mbr = *(MASTER_BOOT_RECORD*)first_sect;

    if (mbr.Signature != MBR_SIGNATURE)
        return false;

    bool has_gpt = false;

    for (const auto& part : mbr.Partition) {
        if (part.OSIndicator == gpt_protective) {
            has_gpt = true;
            break;
        }
    }

    if (!has_gpt)
        return false;

    auto& gpt = *(EFI_PARTITION_TABLE_HEADER*)(first_sect + 0x200);

    // FIXME - if GPT not valid, get alternative version from end of disk

    if (gpt.Header.Signature != gpt_signature) {
        print(u"Invalid GPT signature\r\n");
        return false;
    }

    // FIXME - check header CRC32

    if (gpt.MyLBA != 1) {
        print(u"Invalid LBA value in GPT\r\n");
        return false;
    }

    if (gpt.NumberOfPartitionEntries == 0)
        return false;

    if (gpt.SizeOfPartitionEntry < 0x80) {
        print(u"partition entry size was 0x");
        print_hex(gpt.SizeOfPartitionEntry);
        print(u", expected at least 0x80\r\n");
        return false;
    }

    size_t table_start = gpt.PartitionEntryLBA * biop->Media->BlockSize;
    size_t table_end = table_start + (gpt.SizeOfPartitionEntry * gpt.NumberOfPartitionEntries);
    size_t table_start_aligned = table_start & ~(read_sector_size - 1);
    size_t table_end_aligned = table_end & ~(read_sector_size - 1) ? ((table_end & ~(read_sector_size - 1)) + read_sector_size) : table_end;

    uint8_t* buf;

    status = bs->AllocatePool(EfiLoaderData, table_end_aligned - table_start_aligned, (void**)&buf);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePool", status);
        return false;
    }

    status = biop->ReadBlocks(biop, biop->Media->MediaId, table_start_aligned / biop->Media->BlockSize,
                              table_end_aligned - table_start_aligned, buf);
    if (EFI_ERROR(status)) {
        print_error(u"ReadBlocks", status);
        bs->FreePool(buf);
        return false;
    }

    auto ptr = buf + table_start - table_start_aligned;

    // FIXME - check CRC32 of partition list

    bool found = false;

    for (unsigned int i = 0; i < gpt.NumberOfPartitionEntries; i++) {
        auto& part = *(EFI_PARTITION_ENTRY*)ptr;

        if (!memcmp(&part.PartitionTypeGUID, &moxnox_partition_guid, sizeof(EFI_GUID))) {
            memcpy(sig, &part.UniquePartitionGUID, sizeof(EFI_GUID));
            found = true;
            break;
        }

        ptr += gpt.SizeOfPartitionEntry;
    }

    bs->FreePool(buf);

    return found;
}

static EFI_STATUS select_partition(EFI_HANDLE image_handle, EFI_GUID* sig) {
    EFI_GUID guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_HANDLE* handles = nullptr;
    UINTN count;
    EFI_STATUS status;

    status = bs->LocateHandleBuffer(ByProtocol, &guid, nullptr, &count, &handles);
    if (EFI_ERROR(status)) {
        print_error(u"LocateHandleBuffer", status);
        return status;
    }

    // FIXME - if multiple found, allow user to select from list

    for (unsigned int i = 0; i < count; i++) {
        EFI_BLOCK_IO_PROTOCOL* biop = nullptr;

        status = bs->OpenProtocol(handles[i], &guid, (void**)&biop, image_handle, nullptr,
                                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        if (EFI_ERROR(status)) {
            print_error(u"OpenProtocool", status);
            continue;
        }

        if (biop->Media->MediaPresent && !biop->Media->LogicalPartition) {
            if (try_disk(biop, sig)) {
                bs->CloseProtocol(handles[i], &guid, image_handle, nullptr);
                bs->FreePool(handles);
                return EFI_SUCCESS;
            }
        }

        bs->CloseProtocol(handles[i], &guid, image_handle, nullptr);
    }

    bs->FreePool(handles);

    if (count == 0)
        print(u"No disks found.\r\n");

    return EFI_NOT_FOUND;
}

static EFI_STATUS find_partition(EFI_HANDLE image_handle, EFI_GUID* sig, EFI_SIMPLE_FILE_SYSTEM_PROTOCOL** pfs) {
    EFI_GUID guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_GUID devp_guid = EFI_DEVICE_PATH_PROTOCOL_GUID;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_HANDLE* handles = nullptr;
    UINTN count;
    EFI_STATUS status;

    status = bs->LocateHandleBuffer(ByProtocol, &guid, nullptr, &count, &handles);
    if (EFI_ERROR(status)) {
        print_error(u"LocateHandleBuffer", status);
        return status;
    }

    for (unsigned int i = 0; i < count; i++) {
        EFI_DEVICE_PATH_PROTOCOL* dpp = nullptr;
        EFI_GUID* part_sig = nullptr;

        status = bs->OpenProtocol(handles[i], &devp_guid, (void**)&dpp, image_handle, nullptr,
                                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        if (EFI_ERROR(status)) {
            print_error(u"OpenProtocol", status);
            continue;
        }

        while (true) {
            if (dpp->Type == MEDIA_DEVICE_PATH && dpp->SubType == MEDIA_HARDDRIVE_DP) {
                auto& hddp = *(HARDDRIVE_DEVICE_PATH*)dpp;

                if (hddp.SignatureType == SIGNATURE_TYPE_GUID) {
                    part_sig = (EFI_GUID*)hddp.Signature;
                    break;
                }
            }

            if (dpp->Type == END_DEVICE_PATH_TYPE)
                break;

            dpp = (EFI_DEVICE_PATH_PROTOCOL*)((uint8_t*)dpp + *(uint16_t*)(dpp->Length));
        }

        if (part_sig && !memcmp(part_sig, sig, sizeof(EFI_GUID))) {
            EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs = nullptr;

            bs->CloseProtocol(handles[i], &guid, image_handle, nullptr);

            status = bs->OpenProtocol(handles[i], &fs_guid, (void**)&fs, image_handle, nullptr,
                                      EFI_OPEN_PROTOCOL_GET_PROTOCOL);
            if (EFI_ERROR(status)) {
                print(u"Partition found, but can't open filesystem.\r\n");
                print_error(u"OpenProtocol", status);
                bs->FreePool(handles);
                return status;
            }

            *pfs = fs;

            bs->FreePool(handles);

            return EFI_SUCCESS;
        }

        bs->CloseProtocol(handles[i], &guid, image_handle, nullptr);
    }

    bs->FreePool(handles);

    if (count == 0)
        print(u"No partitions found.\r\n");

    return EFI_NOT_FOUND;
}

static void wait_for_key() {
    UINTN event;

    bs->WaitForEvent(1, &systable->ConIn->WaitForKey, &event);
}

static EFI_STATUS get_file_size(EFI_FILE_PROTOCOL& f, uint64_t& size) {
    EFI_STATUS status;
    EFI_FILE_INFO file_info;
    EFI_GUID info_type = EFI_FILE_INFO_ID;
    UINTN file_info_size = sizeof(file_info);
    EFI_FILE_INFO* file_info2;

    status = f.GetInfo(&f, &info_type, &file_info_size, &file_info);
    if (!EFI_ERROR(status)) {
        size = file_info.FileSize;
        return EFI_SUCCESS;
    }

    if (status != EFI_BUFFER_TOO_SMALL) {
        print_error(u"GetInfo", status);
        return status;
    }

    status = bs->AllocatePool(EfiLoaderData, file_info_size, (void**)&file_info2);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePool", status);
        return status;
    }

    status = f.GetInfo(&f, &info_type, &file_info_size, file_info2);
    if (EFI_ERROR(status)) {
        print_error(u"GetInfo", status);
        bs->FreePool(file_info2);
        return status;
    }

    size = file_info2->FileSize;

    bs->FreePool(file_info2);

    return EFI_SUCCESS;
}

static EFI_STATUS load_kernel(EFI_FILE_PROTOCOL& root, uint64_t& entry_point, uint64_t& img_pa, size_t& img_size) {
    EFI_STATUS status;
    EFI_FILE_PROTOCOL* kernel;
    EFI_PHYSICAL_ADDRESS addr;
    uint64_t size;
    UINTN read_size;

    status = root.Open(&root, &kernel, (CHAR16*)u"\\kernel.exe", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        print_error(u"Open", status);
        return status;
    }

    status = get_file_size(*kernel, size);
    if (EFI_ERROR(status)) {
        print_error(u"get_file_size", status);
        goto end;
    }

    if (size == 0) {
        print(u"Kernel size was 0.\r\n");
        status = EFI_INVALID_PARAMETER;
        goto end;
    }

    status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, bytes_to_pages(size), &addr);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePages", status);
        goto end;
    }

    read_size = size;

    status = kernel->Read(kernel, &read_size, (void*)addr);
    if (EFI_ERROR(status)) {
        print_error(u"Read", status);
        goto end2;
    }

    status = load_pe((uint8_t*)addr, size, KERNEL_ADDR, entry_point, img_pa, img_size);
    if (EFI_ERROR(status)) {
        print_error(u"load_pe", status);
        goto end2;
    }

    status = EFI_SUCCESS;

end2:
    bs->FreePages(addr, bytes_to_pages(size));

end:
    kernel->Close(kernel);

    return status;
}

static EFI_STATUS go(EFI_HANDLE image_handle, uint64_t entry_point, uint64_t cr3, uint64_t acpi_tables) {
    EFI_STATUS status;
    UINTN size, key, descsize;
    UINT32 version;
    EFI_MEMORY_DESCRIPTOR* mapdesc;

    lb.map_ptr = (uintptr_t)map;
    lb.num_map_entries = num_map_entries;
    lb.rsdt_pa = acpi_tables;
    lb.cmdline = 0; // FIXME

    // exit boot services

    size = 0;

    // get new key
    status = bs->GetMemoryMap(&size, nullptr, &key, &descsize, &version);
    if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL) {
        print_error(u"GetMemoryMap", status);
        return status;
    }

    size *= 2;

    status = bs->AllocatePool(EfiLoaderData, size, (void**)&mapdesc);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePool", status);
        return status;
    }

    status = bs->GetMemoryMap(&size, mapdesc, &key, &descsize, &version);
    if (EFI_ERROR(status)) {
        print_error(u"GetMemoryMap", status);
        return status;
    }

    status = bs->ExitBootServices(image_handle, key);
    if (EFI_ERROR(status)) {
        print_error(u"ExitBootServices", status);
        return status;
    }

    // FIXME - SetVirtualAddressMap

    start_kernel(entry_point, cr3, &lb);

    return EFI_SUCCESS; // never reached
}

static EFI_STATUS find_acpi_tables(uint64_t& addr) {
    EFI_GUID acpi2_guid = ACPI_20_TABLE_GUID;
    RSDP_DESCRIPTOR* rsdp = nullptr;

    for (unsigned int i = 0; i < systable->NumberOfTableEntries; i++) {
        if (!memcmp(&systable->ConfigurationTable[i].VendorGuid, &acpi2_guid, sizeof(EFI_GUID))) {
            rsdp = (RSDP_DESCRIPTOR*)systable->ConfigurationTable[i].VendorTable;
            break;
        }
    }

    if (!rsdp) {
        print(u"ACPI 2.0 tables not found\r\n");
        return EFI_NOT_FOUND;
    }

    if (rsdp->revision == 2)
        addr = rsdp->xsdt_physical_address;
    else {
        print(u"Unrecognized ACPI revision ");
        print_hex(rsdp->revision);
        print(u"\r\n");

        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

static EFI_STATUS init_framebuffer(EFI_HANDLE image_handle) {
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_HANDLE* handles = NULL;
    UINTN count;
    EFI_STATUS status;

    status = bs->LocateHandleBuffer(ByProtocol, &guid, NULL, &count, &handles);
    if (EFI_ERROR(status))
        return status;

    for (unsigned int i = 0; i < count; i++) {
        EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
        unsigned int mode = 0;
        unsigned int pixels = 0;

        status = bs->OpenProtocol(handles[i], &guid, (void**)&gop, image_handle, NULL,
                                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        if (EFI_ERROR(status)) {
            print_error(u"OpenProtocol", status);
            continue;
        }

        for (unsigned int j = 0; j < gop->Mode->MaxMode; j++) {
            UINTN size;
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;

            status = gop->QueryMode(gop, j, &size, &info);
            if (EFI_ERROR(status)) {
                print_error(u"QueryMode", status);
                continue;
            }

            // choose the best mode
            // FIXME - allow user to override this?

            if (info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor &&
                info->HorizontalResolution != info->VerticalResolution && // ignore the square resolutions that Qemu offers
                info->HorizontalResolution * info->VerticalResolution > pixels) {
                mode = j;
                pixels = info->HorizontalResolution * info->VerticalResolution;
            }
        }

        status = gop->SetMode(gop, mode);
        if (EFI_ERROR(status)) {
            print_error(u"SetMode", status);
            bs->CloseProtocol(handles[i], &guid, image_handle, NULL);
            goto end;
        }

        lb.framebuffer.phys_address = (uintptr_t)gop->Mode->FrameBufferBase;
        lb.framebuffer.width = gop->Mode->Info->HorizontalResolution;
        lb.framebuffer.height = gop->Mode->Info->VerticalResolution;
        lb.framebuffer.pixels_per_scan_line = gop->Mode->Info->PixelsPerScanLine;

        bs->CloseProtocol(handles[i], &guid, image_handle, NULL);

        status = EFI_SUCCESS;
        goto end;
    }

    status = EFI_NOT_FOUND;

end:
    bs->FreePool(handles);

    return status;
}

extern "C"
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    EFI_STATUS status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;
    EFI_FILE_PROTOCOL* root;
    uint64_t entry_point, cr3, img_pa, acpi_tables;
    size_t img_size;
    EFI_PHYSICAL_ADDRESS stack;
    EFI_GUID part_sig;

    systable = SystemTable;
    bs = systable->BootServices;

    status = load_efi_drivers(ImageHandle);
    if (EFI_ERROR(status)) {
        print_error(u"load_efi_drivers", status);
        goto end;
    }

    status = select_partition(ImageHandle, &part_sig);
    if (EFI_ERROR(status)) {
        print_error(u"select_partition", status);
        goto end;
    }

    status = find_partition(ImageHandle, &part_sig, &fs);
    if (EFI_ERROR(status)) {
        print_error(u"find_partition", status);
        goto end;
    }

    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) {
        print_error(u"OpenVolume", status);
        goto end;
    }

    status = load_kernel(*root, entry_point, img_pa, img_size);
    if (EFI_ERROR(status)) {
        print_error(u"load_kernel", status);
        root->Close(root);
        goto end;
    }

    root->Close(root);

    status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, KERNEL_STACK_PAGES, &stack);
    if (EFI_ERROR(status)) {
        print_error(u"AllocatePages", status);
        goto end;
    }

    status = find_acpi_tables(acpi_tables);
    if (EFI_ERROR(status)) {
        print_error(u"find_acpi_tables", status);
        goto end;
    }

    status = init_framebuffer(ImageHandle);
    if (EFI_ERROR(status)) {
        print_error(u"init_framebuffer", status);
        goto end;
    }

    status = get_memory_map();
    if (EFI_ERROR(status)) {
        print_error(u"get_memory_map", status);
        goto end;
    }

    status = add_map_entry(img_pa, img_size, mem_type::image);
    if (EFI_ERROR(status)) {
        print_error(u"add_map_entry", status);
        goto end;
    }

    status = add_map_entry(stack, KERNEL_STACK_PAGES << EFI_PAGE_SHIFT, mem_type::stack);
    if (EFI_ERROR(status)) {
        print_error(u"add_map_entry", status);
        goto end;
    }

    status = add_map_entry((uintptr_t)lb.framebuffer.phys_address, bytes_to_pages(lb.framebuffer.pixels_per_scan_line * lb.framebuffer.height * 4) << EFI_PAGE_SHIFT, mem_type::framebuffer);
    if (EFI_ERROR(status)) {
        print_error(u"add_map_entry", status);
        goto end;
    }

    status = setup_paging(cr3);
    if (EFI_ERROR(status)) {
        print_error(u"setup_paging", status);
        goto end;
    }

    // FIXME - pass runtime services list through to kernel

    status = go(ImageHandle, entry_point, cr3, acpi_tables); // doesn't return unless unsuccessful
    print_error(u"go", status);

end:
    wait_for_key();

    return EFI_SUCCESS;
}
