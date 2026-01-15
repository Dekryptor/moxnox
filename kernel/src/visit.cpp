module;

#include <span>
#include <stdint.h>

export module visit;

import nt;
import objects;

using namespace std;

export void object_acquire(object& obj);
export void object_release(object& obj);
export bool object_is_sync_object(object& obj);
export void object_on_wake(object& obj);
export nt_status object_write_without_offset(object& obj, span<const uint8_t> buf,
                                             uint64_t& written);
export nt_status object_write_with_offset(object& obj, span<const uint8_t> s,
                                          uint64_t offset, uint64_t& written_bytes);
export nt_status object_ioctl(object& obj, uint32_t control_code, span<const uint8_t> inbuf,
                              span<uint8_t> outbuf, IO_STATUS_BLOCK& iosb);
export nt_status object_read_with_offset(object& obj, span<uint8_t> s,
                                         uint64_t offset, uint64_t& read_bytes);
export nt_status object_read_without_offset(object& obj, span<uint8_t> s,
                                            uint64_t& read_bytes);
export bool object_is_exclusive(object& obj);
export sync_object& object_get_sync_object(object& obj);
export void object_last_handle_closed(object& obj);
