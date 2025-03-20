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

template<typename T>
T& tree_containing_record(rbtree_node* tree) {
    return *(T*)((uint8_t*)tree - offsetof(T, tree));
}

rbtree_node* find_rbtree_node(rbtree_node* root, uint64_t key);
rbtree_node* get_tree_minimum(rbtree_node* root);
void insert_into_tree(rbtree_node*& root, rbtree_node& ent);
rbtree_node* next_rbtree_node(const rbtree_node& ent);
