export module mapping_type;

export enum class mapping_type {
    no_access,
    read_write,
    read_only,
    read_execute,
    read_write_execute,
    no_cache,
    write_combine,
    reserved
};
