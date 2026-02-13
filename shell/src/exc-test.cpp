#include <iostream>
#include <stdexcept>
#include <string>
#include <memory>

// Test class with destructor to verify stack unwinding
class ResourceHolder {
public:
    ResourceHolder(const char* name) : name_(name) {
        std::cout << "ResourceHolder(" << name_ << ") constructed\n";
    }
    
    ~ResourceHolder() {
        std::cout << "ResourceHolder(" << name_ << ") destroyed\n";
    }
    
private:
    const char* name_;
};

// Function that throws
void throw_something(int type) {
    ResourceHolder rh("in throw_something");
    
    switch(type) {
        case 1:
            throw std::runtime_error("runtime_error test");
        case 2:
            throw std::logic_error("logic_error test");
        case 3:
            throw 42;
        case 4:
            throw std::string("string exception");
        default:
            throw "char* exception";
    }
}

// Nested function calls to test stack unwinding
void level3(int type) {
    ResourceHolder rh("level3");
    throw_something(type);
}

void level2(int type) {
    ResourceHolder rh("level2");
    level3(type);
}

void level1(int type) {
    ResourceHolder rh("level1");
    level2(type);
}

int main() {
    std::cout << "=== C++ Exception Test ===\n\n";
    
    // Test 1: Basic catch
    std::cout << "Test 1: Basic exception catch\n";
    try {
        throw std::runtime_error("basic test");
    } catch (const std::exception& e) {
        std::cout << "Caught: " << e.what() << "\n\n";
    }
    
    // Test 2: Stack unwinding with destructors
    std::cout << "Test 2: Stack unwinding (watch destructors)\n";
    try {
        ResourceHolder rh("main");
        level1(1);
    } catch (const std::exception& e) {
        std::cout << "Caught: " << e.what() << "\n\n";
    }
    
    // Test 3: Multiple catch blocks
    std::cout << "Test 3: Multiple catch blocks\n";
    for (int i = 1; i <= 5; i++) {
        try {
            throw_something(i);
        } catch (const std::runtime_error& e) {
            std::cout << "Caught runtime_error: " << e.what() << "\n";
        } catch (const std::logic_error& e) {
            std::cout << "Caught logic_error: " << e.what() << "\n";
        } catch (int n) {
            std::cout << "Caught int: " << n << "\n";
        } catch (const std::string& s) {
            std::cout << "Caught string: " << s << "\n";
        } catch (...) {
            std::cout << "Caught unknown exception\n";
        }
    }
    std::cout << "\n";
    
    // Test 4: Nested try-catch
    std::cout << "Test 4: Nested try-catch\n";
    try {
        try {
            throw std::runtime_error("inner");
        } catch (const std::logic_error& e) {
            std::cout << "Inner catch (shouldn't reach)\n";
        }
    } catch (const std::runtime_error& e) {
        std::cout << "Outer catch: " << e.what() << "\n\n";
    }
    
    // Test 5: Rethrowing
    std::cout << "Test 5: Rethrow\n";
    try {
        try {
            throw std::runtime_error("rethrow test");
        } catch (...) {
            std::cout << "Caught in inner, rethrowing...\n";
            throw;  // rethrow
        }
    } catch (const std::exception& e) {
        std::cout << "Caught in outer: " << e.what() << "\n\n";
    }
    
    // Test 6: Exception in constructor
    std::cout << "Test 6: Exception in constructor\n";
    class ThrowInConstructor {
    public:
        ThrowInConstructor() {
            throw std::runtime_error("constructor exception");
        }
    };
    
    try {
        ThrowInConstructor t;
    } catch (const std::exception& e) {
        std::cout << "Caught: " << e.what() << "\n\n";
    }
    
    // Test 7: std::unique_ptr cleanup during unwinding
    std::cout << "Test 7: Smart pointer cleanup\n";
    try {
        auto ptr = std::make_unique<ResourceHolder>("unique_ptr resource");
        throw std::runtime_error("with unique_ptr");
    } catch (const std::exception& e) {
        std::cout << "Caught: " << e.what() << "\n\n";
    }
    
    std::cout << "=== All tests passed! ===\n";
    return 0;
}
