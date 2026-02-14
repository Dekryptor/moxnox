export module rwsem;

import list;

export class rwsem {
public:
    void lock();
    void unlock();
    void lock_shared();
    void unlock_shared();

    int shared_count = 0;
    bool exclusive = false;
    list_entry wait_list_shared = { &wait_list_shared, &wait_list_shared };
    list_entry wait_list_exclusive = { &wait_list_exclusive, &wait_list_exclusive };
};
