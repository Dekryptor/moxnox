module;

#include <stdint.h>
#include <stddef.h>

export module rbtree;

export enum class tree_colour {
    red,
    black
};

export struct rbtree_node {
    rbtree_node* parent;
    rbtree_node* left;
    rbtree_node* right;
    enum tree_colour colour;
    uint64_t key;
};

export template<typename T>
T& rbtree_containing_record(rbtree_node* tree) {
    return *(T*)((uint8_t*)tree - offsetof(T, tree));
}

export rbtree_node* find_rbtree_node(rbtree_node* root, uint64_t key) {
    rbtree_node* ret = root;

    while (ret && ret->key != key) {
        if (key < ret->key)
            ret = ret->left;
        else
            ret = ret->right;
    }

    return ret;
}

export rbtree_node* get_tree_minimum(rbtree_node* root) {
    rbtree_node* ret = root;

    while (ret->left) {
        ret = ret->left;
    }

    return ret;
}

static void left_rotate(rbtree_node*& root, rbtree_node* x) {
    auto y = x->right;

    x->right = y->left;

    if (y->left)
        y->left->parent = x;

    y->parent = x->parent;

    if (!x->parent)
        root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;

    y->left = x;
    x->parent = y;
}

static void right_rotate(rbtree_node*& root, rbtree_node* x) {
    auto y = x->left;

    x->left = y->right;

    if (y->right)
        y->right->parent = x;

    y->parent = x->parent;

    if (!x->parent)
        root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;

    y->right = x;
    x->parent = y;
}

static void insert_fixup(rbtree_node*& root, rbtree_node* z) {
    while (z->parent && z->parent->colour == tree_colour::red) {
        if (z->parent == z->parent->parent->left) {
            auto y = z->parent->parent->right;

            if (y && y->colour == tree_colour::red) {
                y->colour = tree_colour::black;
                z->parent->colour = tree_colour::black;
                z->parent->parent->colour = tree_colour::red;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    left_rotate(root, z);
                }

                z->parent->colour = tree_colour::black;
                z->parent->parent->colour = tree_colour::red;
                right_rotate(root, z->parent->parent);
            }
        } else {
            auto y = z->parent->parent->left;

            if (y && y->colour == tree_colour::red) {
                y->colour = tree_colour::black;
                z->parent->colour = tree_colour::black;
                z->parent->parent->colour = tree_colour::red;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    right_rotate(root, z);
                }

                z->parent->colour = tree_colour::black;
                z->parent->parent->colour = tree_colour::red;
                left_rotate(root, z->parent->parent);
            }
        }
    }

    root->colour = tree_colour::black;
}

export void insert_into_rbtree(rbtree_node*& root, rbtree_node& ent) {
    rbtree_node* ptr = root;
    rbtree_node* new_parent = nullptr;

    while (ptr) {
        new_parent = ptr;

        if (ent.key < ptr->key)
            ptr = ptr->left;
        else
            ptr = ptr->right;
    }

    ent.parent = new_parent;
    ent.left = nullptr;
    ent.right = nullptr;
    ent.colour = tree_colour::red;

    if (!new_parent)
        root = &ent;
    else if (ent.key < new_parent->key)
        new_parent->left = &ent;
    else
        new_parent->right = &ent;

    insert_fixup(root, &ent);
}

export rbtree_node* next_rbtree_node(const rbtree_node& ent) {
    rbtree_node* ret;

    if (ent.right) {
        ret = ent.right;

        while (ret->left) {
            ret = ret->left;
        }
    } else {
        const rbtree_node* last_ent = &ent;

        ret = last_ent->parent;
        while (ret && ret->right == last_ent) {
            last_ent = ret;
            ret = ret->parent;
        }
    }

    return ret;
}
