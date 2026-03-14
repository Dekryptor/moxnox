#include <iostream>
#include <stdexcept>
#include <string>
#include <memory>

using namespace std;

// Test class with destructor to verify stack unwinding
class ResourceHolder {
public:
    ResourceHolder(const char* name) : name_(name) {
        cout << "ResourceHolder(" << name_ << ") constructed\n";
    }

    ~ResourceHolder() {
        cout << "ResourceHolder(" << name_ << ") destroyed\n";
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
    cout << "=== C++ Exception Test ===\n\n";

    // Test 1: Basic catch
    cout << "Test 1: Basic exception catch\n";
    try {
        throw runtime_error("basic test");
    } catch (const exception& e) {
        cout << "Caught: " << e.what() << "\n\n";
    }

    // Test 2: Stack unwinding with destructors
    cout << "Test 2: Stack unwinding (watch destructors)\n";
    try {
        ResourceHolder rh("main");
        level1(1);
    } catch (const exception& e) {
        cout << "Caught: " << e.what() << "\n\n";
    }

    // Test 3: Multiple catch blocks
    cout << "Test 3: Multiple catch blocks\n";
    for (unsigned int i = 1; i <= 5; i++) {
        try {
            throw_something(i);
        } catch (const runtime_error& e) {
            cout << "Caught runtime_error: " << e.what() << "\n";
        } catch (const logic_error& e) {
            cout << "Caught logic_error: " << e.what() << "\n";
        } catch (int n) {
            cout << "Caught int: " << n << "\n";
        } catch (const string& s) {
            cout << "Caught string: " << s << "\n";
        } catch (...) {
            cout << "Caught unknown exception\n";
        }
    }
    cout << "\n";

    // Test 4: Nested try-catch
    cout << "Test 4: Nested try-catch\n";
    try {
        try {
            throw runtime_error("inner");
        } catch (const logic_error& e) {
            cout << "Inner catch (shouldn't reach)\n";
        }
    } catch (const runtime_error& e) {
        cout << "Outer catch: " << e.what() << "\n\n";
    }

    // Test 5: Rethrowing
    cout << "Test 5: Rethrow\n";
    try {
        try {
            throw runtime_error("rethrow test");
        } catch (...) {
            cout << "Caught in inner, rethrowing...\n";
            throw;  // rethrow
        }
    } catch (const exception& e) {
        cout << "Caught in outer: " << e.what() << "\n\n";
    }

    // Test 6: Exception in constructor
    cout << "Test 6: Exception in constructor\n";
    class ThrowInConstructor {
    public:
        ThrowInConstructor() {
            throw runtime_error("constructor exception");
        }
    };

    try {
        ThrowInConstructor t;
    } catch (const exception& e) {
        cout << "Caught: " << e.what() << "\n\n";
    }

    // Test 7: unique_ptr cleanup during unwinding
    cout << "Test 7: Smart pointer cleanup\n";
    try {
        auto ptr = make_unique<ResourceHolder>("unique_ptr resource");
        throw runtime_error("with unique_ptr");
    } catch (const exception& e) {
        cout << "Caught: " << e.what() << "\n\n";
    }

    cout << "=== All tests passed! ===\n";
    return 0;
}
