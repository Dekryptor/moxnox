#include <typeinfo>

import print;

class Base {
public:
    virtual ~Base() {}
};

class Derived : public Base {};

int main() {
    Base* b = new Derived();

    print("Type: {}\n", typeid(*b).name());

    auto d = dynamic_cast<Derived*>(b);
    if (d)
        print("dynamic_cast succeeded\n");
    else
        print("dynamic_cast failed\n");

    if (typeid(*b) == typeid(Derived))
        print("type comparison succeeded\n");
    else
        print("type comparison failed\n");

    delete b;
    return 0;
}
