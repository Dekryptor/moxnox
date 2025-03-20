#pragma once

#include <stdint.h>
#include <stddef.h>

enum class tree_colour {
    red,
    black
};

struct rbtree_node {
    rbtree_node* parent;
    rbtree_node* left;
    rbtree_node* right;
    enum tree_colour colour;
    uint64_t key;
};

