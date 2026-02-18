//===- SaltOps.td - Salt Dialect Operations ----------------*- tablegen -*-===//
//
// Minimal Salt Dialect for Verification.
// Relies on 'func.func' for structure and 'arith' for math.
//
//===----------------------------------------------------------------------===//

include "mlir/IR/OpBase.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/Interfaces/CallInterfaces.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

//===----------------------------------------------------------------------===//
// Salt Dialect Definition
//===----------------------------------------------------------------------===//

def Salt_Dialect : Dialect {
  let name = "salt";
  let summary = "Verified Systems Programming Dialect";
  let cppNamespace = "::salt";
  // Disable default printing/parsing to avoid Bytecode interface requirements for now
  let useDefaultTypePrinterParser = 0; 
}

//===----------------------------------------------------------------------===//
// Base Class
//===----------------------------------------------------------------------===//

class Salt_Op<string mnemonic, list<Trait> traits = []> :
    Op<Salt_Dialect, mnemonic, traits>;

//===----------------------------------------------------------------------===//
// Verification Operations (Z3 Only)
//===----------------------------------------------------------------------===//

def VerifyCheckOp : Salt_Op<"verify"> {
  let summary = "A formal contract assertion";
  let description = [{
    Represents a 'requires' or 'ensures' clause. 
    Consumed by Z3VerifyPass. Erased before CodeGen.
  }];
  let arguments = (ins StrAttr:$kind, Variadic<AnyType>:$args);
  let assemblyFormat = "$kind `(` $args `)` attr-dict `:` functional-type($args, results)";
}

//===----------------------------------------------------------------------===//
// Execution Operations
//===----------------------------------------------------------------------===//

def CallOp : Salt_Op<"call", [
    DeclareOpInterfaceMethods<SymbolUserOpInterface>
  ]> {
  let summary = "Function call with implicit region forwarding";
  let arguments = (ins FlatSymbolRefAttr:$callee, Variadic<AnyType>:$operands);
  let results = (outs Variadic<AnyType>);
  
  let assemblyFormat = [{
    $callee `(` $operands `)` attr-dict `:` functional-type($operands, results)
  }];
}

//===----------------------------------------------------------------------===//
// Bare Metal (Optional but simple)
//===----------------------------------------------------------------------===//

def AsmOp : Salt_Op<"asm"> {
  let summary = "Inline Assembly for Kernel Privileges";
  let arguments = (ins StrAttr:$asm_string, StrAttr:$constraints, Variadic<AnyType>:$operands);
  let results = (outs Variadic<AnyType>:$results);
  let assemblyFormat = "$asm_string `,` $constraints `(` $operands `)` attr-dict `:` functional-type($operands, $results)";
}

def PanicOp : Salt_Op<"panic", [Terminator]> {
  let summary = "Crash the kernel";
  let arguments = (ins StrAttr:$message);
  let assemblyFormat = "$message attr-dict";
}
