#pragma once

import list;

struct rangelock {
    list_entry active = { &active, &active };
    list_entry waiting = { &waiting, &waiting };
};
