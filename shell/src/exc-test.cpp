#include <stdexcept>
#include <string>
#include <memory>

import print;

using namespace std;

// Test class with destructor to verify stack unwinding
class ResourceHolder {
public:
    ResourceHolder(const char* name) : name_(name) {
        print("ResourceHolder({}) constructed\n", name_);
    }

    ~ResourceHolder() {
        print("ResourceHolder({}) destroyed\n", name_);
    }

private:
    const char* name_;
};

// Function that throws
void throw_something(unsigned int type) {
    ResourceHolder rh("in throw_something");

    switch(type) {
        case 1:
            throw runtime_error("runtime_error test");
        case 2:
            throw logic_error("logic_error test");
        case 3:
            throw 42;
        case 4:
            throw string("string exception");
        default:
            throw "char* exception";
    }
}

// Nested function calls to test stack unwinding
void level3(unsigned int type) {
    ResourceHolder rh("level3");
    throw_something(type);
}

void level2(unsigned int type) {
    ResourceHolder rh("level2");
    level3(type);
}

void level1(unsigned int type) {
    ResourceHolder rh("level1");
    level2(type);
}

int main() {
    print("=== C++ Exception Test ===\n\n");

    // Test 1: Basic catch
    print("Test 1: Basic exception catch\n");
    try {
        throw runtime_error("basic test");
    } catch (const exception& e) {
        print("Caught: {}\n\n", e.what());
    }

    // Test 2: Stack unwinding with destructors
    print("Test 2: Stack unwinding (watch destructors)\n");
    try {
        ResourceHolder rh("main");
        level1(1);
    } catch (const exception& e) {
        print("Caught: {}\n\n", e.what());
    }

    // Test 3: Multiple catch blocks
    print("Test 3: Multiple catch blocks\n");
    for (unsigned int i = 1; i <= 5; i++) {
        try {
            throw_something(i);
        } catch (const runtime_error& e) {
            print("Caught runtime_error: {}\n", e.what());
        } catch (const logic_error& e) {
            print("Caught logic_error: {}\n", e.what());
        } catch (int n) {
            print("Caught int: {}\n", n);
        } catch (const string& s) {
            print("Caught string: {}\n", string_view(s));
        } catch (...) {
            print("Caught unknown exception\n");
        }
    }
    print("\n");

    // Test 4: Nested try-catch
    print("Test 4: Nested try-catch\n");
    try {
        try {
            throw runtime_error("inner");
        } catch (const logic_error& e) {
            print("Inner catch (shouldn't reach)\n");
        }
    } catch (const runtime_error& e) {
        print("Outer catch: {}\n\n", e.what());
    }

    // Test 5: Rethrowing
    print("Test 5: Rethrow\n");
    try {
        try {
            throw runtime_error("rethrow test");
        } catch (...) {
            print("Caught in inner, rethrowing...\n");
            throw;  // rethrow
        }
    } catch (const exception& e) {
        print("Caught in outer: {}\n\n", e.what());
    }

    // Test 6: Exception in constructor
    print("Test 6: Exception in constructor\n");
    class ThrowInConstructor {
    public:
        ThrowInConstructor() {
            throw runtime_error("constructor exception");
        }
    };

    try {
        ThrowInConstructor t;
    } catch (const exception& e) {
        print("Caught: {}\n\n", e.what());
    }

    // Test 7: unique_ptr cleanup during unwinding
    print("Test 7: Smart pointer cleanup\n");
    try {
        auto ptr = make_unique<ResourceHolder>("unique_ptr resource");
        throw runtime_error("with unique_ptr");
    } catch (const exception& e) {
        print("Caught: {}\n\n", e.what());
    }

    print("=== All tests passed! ===\n");
    return 0;
}
