// Minimal in-process JIT smoke test for llvmpipe bring-up: builds a function
// returning 42 in IR, JITs it via ORC LLJIT (the engine gallivm uses with
// GALLIVM_USE_ORCJIT), calls it and prints the result.
//
// Not part of the build; compile by hand against the llvm-native dest:
//
// x86_64-pc-moxnox-clang++ -std=c++20 -fno-exceptions -fno-rtti -O1 \
//   -stdlib=libc++ -static-libstdc++ \
//   -I build/ports/llvm/dest/usr/include jit-test.cpp -o jit-test.exe \
//   -Wl,--start-group build/ports/llvm/dest/usr/lib/libLLVM*.a -Wl,--end-group

#include <cstdio>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>

using namespace llvm;
using namespace llvm::orc;

int main() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    auto ctx = std::make_unique<LLVMContext>();
    auto mod = std::make_unique<Module>("jit-test", *ctx);

    auto* fn_type = FunctionType::get(Type::getInt32Ty(*ctx), false);
    auto* fn = Function::Create(fn_type, Function::ExternalLinkage, "answer",
                                mod.get());
    IRBuilder<> b(BasicBlock::Create(*ctx, "entry", fn));
    b.CreateRet(b.getInt32(42));

    printf("creating LLJIT...\n");

    auto jit = LLJITBuilder().create();
    if (!jit) {
        fprintf(stderr, "LLJITBuilder: %s\n",
                toString(jit.takeError()).c_str());
        return 1;
    }

    printf("triple: %s\n",
           (*jit)->getTargetTriple().str().c_str());

    if (auto err = (*jit)->addIRModule(
            ThreadSafeModule(std::move(mod), std::move(ctx)))) {
        fprintf(stderr, "addIRModule: %s\n", toString(std::move(err)).c_str());
        return 1;
    }

    printf("looking up...\n");

    auto sym = (*jit)->lookup("answer");
    if (!sym) {
        fprintf(stderr, "lookup: %s\n", toString(sym.takeError()).c_str());
        return 1;
    }

    auto* answer = sym->toPtr<int (*)()>();

    printf("calling JITted function...\n");

    auto ret = answer();

    printf("JITted function returned %d\n", ret);

    return ret == 42 ? 0 : 1;
}
