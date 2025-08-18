module;

#include <stdint.h>
#include <stddef.h>

export module rbtree;

export enum class tree_colour {
    red,
    black
};

export template<typename KeyType>
struct rbtree_node {
    rbtree_node<KeyType>* parent;
    rbtree_node<KeyType>* left;
    rbtree_node<KeyType>* right;
    enum tree_colour colour;
    KeyType key;
};

export template<typename T, typename KeyType>
T& rbtree_containing_record(rbtree_node<KeyType>* tree) {
    return *(T*)((uint8_t*)tree - offsetof(T, tree));
}

export
template<typename KeyType>
rbtree_node<KeyType>* find_rbtree_node(rbtree_node<KeyType>* root, KeyType key) {
    rbtree_node<KeyType>* ret = root;

    while (ret && ret->key != key) {
        if (key < ret->key)
            ret = ret->left;
        else
            ret = ret->right;
    }

    if (ret && ret->key != key)
        return nullptr;

    return ret;
}

export
template<typename KeyType>
const rbtree_node<KeyType>* find_rbtree_node(const rbtree_node<KeyType>* root, KeyType key) {
    const rbtree_node<KeyType>* ret = root;

    while (ret && ret->key != key) {
        if (key < ret->key)
            ret = ret->left;
        else
            ret = ret->right;
    }

    if (ret && ret->key != key)
        return nullptr;

    return ret;
}

export
template<typename KeyType>
rbtree_node<KeyType>* find_rbtree_node_or_less(rbtree_node<KeyType>* root, KeyType key) {
    rbtree_node<KeyType>* ret = root;

    while (ret && ret->key != key) {
        if (key < ret->key) {
            if (!ret->left)
                return ret;

            ret = ret->left;
        } else {
            if (!ret->right)
                return ret;

            ret = ret->right;
        }
    }

    return ret;
}

export
template<typename KeyType>
const rbtree_node<KeyType>* find_rbtree_node_or_less(const rbtree_node<KeyType>* root, KeyType key) {
    const rbtree_node<KeyType>* ret = root;

    while (ret && ret->key != key) {
        if (key < ret->key) {
            if (!ret->left)
                return ret;

            ret = ret->left;
        } else {
            if (!ret->right)
                return ret;

            ret = ret->right;
        }
    }

    return ret;
}

export
template<typename KeyType>
rbtree_node<KeyType>* get_tree_minimum(rbtree_node<KeyType>* root) {
    rbtree_node<KeyType>* ret = root;

    while (ret->left) {
        ret = ret->left;
    }

    return ret;
}

export
template<typename KeyType>
const rbtree_node<KeyType>* get_tree_minimum(const rbtree_node<KeyType>* root) {
    const rbtree_node<KeyType>* ret = root;

    while (ret->left) {
        ret = ret->left;
    }

    return ret;
}

template<typename KeyType>
void left_rotate(rbtree_node<KeyType>*& root, rbtree_node<KeyType>* x) {
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

template<typename KeyType>
void right_rotate(rbtree_node<KeyType>*& root, rbtree_node<KeyType>* x) {
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

template<typename KeyType>
void insert_fixup(rbtree_node<KeyType>*& root, rbtree_node<KeyType>* z) {
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

export
template<typename KeyType>
void insert_into_rbtree(rbtree_node<KeyType>*& root, rbtree_node<KeyType>& ent) {
    rbtree_node<KeyType>* ptr = root;
    rbtree_node<KeyType>* new_parent = nullptr;

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

export
template<typename KeyType>
rbtree_node<KeyType>* next_rbtree_node(const rbtree_node<KeyType>& ent) {
    rbtree_node<KeyType>* ret;

    if (ent.right) {
        ret = ent.right;

        while (ret->left) {
            ret = ret->left;
        }
    } else {
        const rbtree_node<KeyType>* last_ent = &ent;

        ret = last_ent->parent;
        while (ret && ret->right == last_ent) {
            last_ent = ret;
            ret = ret->parent;
        }
    }

    return ret;
}

template<typename KeyType>
void rb_transplant(rbtree_node<KeyType>*& root, rbtree_node<KeyType>& u,
                   rbtree_node<KeyType>* v) {
    if (!u.parent)
        root = v;
    else if (&u == u.parent->left)
        u.parent->left = v;
    else
        u.parent->right = v;

    if (v)
        v->parent = u.parent;
}

template<typename KeyType>
void rb_delete_fixup(rbtree_node<KeyType>*& root, rbtree_node<KeyType>* x) {
    while (x && x != root && x->colour == tree_colour::black) {
        if (x == x->parent->left) {
            auto* w = x->parent->right;

            if (w->colour == tree_colour::red) {
                w->colour = tree_colour::black;
                x->parent->colour = tree_colour::red;
                left_rotate(root, x->parent);
                w = x->parent->right;
            }

            if (w->left->colour == tree_colour::black && w->right->colour == tree_colour::black) {
                w->colour = tree_colour::red;
                x = x->parent;
            } else {
                if (w->right->colour == tree_colour::black) {
                    w->left->colour = tree_colour::black;
                    w->colour = tree_colour::red;
                    right_rotate(root, w);
                    w = x->parent->right;
                }

                w->colour = x->parent->colour;
                x->parent->colour = tree_colour::black;
                w->right->colour = tree_colour::black;
                left_rotate(root, x->parent);
                x = root;
            }
        } else {
            auto* w = x->parent->left;

            if (w->colour == tree_colour::red) {
                w->colour = tree_colour::black;
                x->parent->colour = tree_colour::red;
                right_rotate(root, x->parent);
                w = x->parent->left;
            }

            if (w->left->colour == tree_colour::black && w->right->colour == tree_colour::black) {
                w->colour = tree_colour::red;
                x = x->parent;
            } else {
                if (w->left->colour == tree_colour::black) {
                    w->right->colour = tree_colour::black;
                    w->colour = tree_colour::red;
                    left_rotate(root, w);
                    w = x->parent->left;
                }

                w->colour = x->parent->colour;
                x->parent->colour = tree_colour::black;
                w->left->colour = tree_colour::black;
                right_rotate(root, x->parent);
                x = root;
            }
        }
    }

    if (x)
        x->colour = tree_colour::black;
}

export
template<typename KeyType>
void remove_from_rbtree(rbtree_node<KeyType>*& root, rbtree_node<KeyType>& ent) {
    rbtree_node<KeyType>* x;
    rbtree_node<KeyType>* y = &ent;

    auto y_orig_colour = ent.colour;

    if (!ent.left) {
        x = ent.right;
        rb_transplant(root, ent, ent.right);
    } else if (!ent.right) {
        x = ent.left;
        rb_transplant(root, ent, ent.left);
    } else {
        y = get_tree_minimum(ent.right);
        y_orig_colour = y->colour;

        x = y->right;

        if (y->parent == &ent) {
            if (x)
                x->parent = y;
        } else {
            rb_transplant(root, *y, y->right);
            y->right = ent.right;
            y->right->parent = y;
        }

        rb_transplant(root, ent, y);
        y->left = ent.left;
        y->left->parent = y;
        y->colour = ent.colour;
    }

    if (y_orig_colour == tree_colour::black)
        rb_delete_fixup(root, x);
}
