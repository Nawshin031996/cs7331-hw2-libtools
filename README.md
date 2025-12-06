
# CS7331 – Homework 2: LLVM LibTooling

This repository contains two LLVM LibTools developed for Homework 2:

1. **func-analyzer** – analyzes function definitions in a C/C++ source file  
2. **strength-reducer** – performs simple operator strength reduction  

Both tools were implemented using **RecursiveASTVisitor**, **ASTConsumer**, and **Clang Rewriter** following the guidelines of the assignment.

---

# 1. func-analyzer

### Purpose

`func-analyzer` traverses a C/C++ source file and extracts the following information for every **function definition**:

- Function name  
- Number of arguments  
- Number of statements in the function body  
- Number of loops in the function body (`for`, `while`, `do`)  
- Number of times the function is called within the same source file  

---

## How It Works (High-Level Explanation)

The tool uses two separate AST visitors:

### **1. FunctionDefVisitor**
Visits `FunctionDecl` nodes with bodies, and extracts:

- `func->getQualifiedNameAsString()` → function name  
- `func->param_size()` → number of parameters  
- Recursively visits all statements in the body to count:
  - total statements (`Stmt` nodes)  
  - loop constructs (`ForStmt`, `WhileStmt`, `DoStmt`)  

### **2. FunctionCallVisitor**
Visits every `CallExpr` node and:

- Retrieves the callee using `getDirectCallee()`  
- Converts the callee to a string name  
- Increments a counter for that function  

### **ASTConsumer**
- Restricts analysis only to the *main source file* using `SourceManager::getMainFileID()`  
- Runs both visitors  
- Produces a final report with all collected metrics  

---

## Example Output
```text
Function: add_twice
Number of arguments: 1
Number of statements: 23
Number of loops: 1
Times called in file: 1

Function: main
Number of arguments: 0
Number of statements: 34
Number of loops: 0
Times called in file: 0

Function: printf
Number of arguments: 0
Number of statements: 0
Number of loops: 0
Times called in file: 1
```

# 2. strength-reducer

###  Purpose

This tool performs simple strength reduction by replacing:

- `x * 2^k` → `x << k`  
- `x / 2^k` → `x >> k`  

This improves performance by replacing expensive multiplication/division with fast bit shifts.

---

## How It Works (High-Level Explanation)

- Visits `BinaryOperator` nodes  
- Checks:  
  - operator is `*` or `/`  
  - RHS is an integer literal  
  - RHS is a power of two  
  - LHS is a simple variable (`DeclRefExpr`)  
- Computes `k = log2(rhs_value)`  
- Constructs replacement expression (`lhs << k` or `lhs >> k`)  
- Uses `clang::Rewriter` to apply the transformation  
- Prints rewritten output in `EndSourceFileAction()`  

Example transformation:

### Input

```c
int c = a * 4;
int d = a / 2;

Output
int c = a << 2;
int d = a >> 1;
``` 
# 3. How to Build These Tools With LLVM (What I Did on Lonestar6)

To integrate the tools with LLVM:

### 1. Copy each tool folder into:
```bash
llvm-project/clang-tools-extra/

```
### 2. Modify the clang-tools-extra/CMakeLists.txt:
```cmake
add_subdirectory(func-analyzer)
add_subdirectory(strength-reducer)

```
### 3. Create a build folder:
```bash
mkdir -p /work/11036/nawshin03/ls6/llvm-build
cd /work/11036/nawshin03/ls6/llvm-build

```

### 4. Configure LLVM for Clang + Extra Tools:
```bash
module load gcc
module load cmake

cmake -G "Unix Makefiles" \
  -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" \
  -DCMAKE_BUILD_TYPE=Release \
  /work/11036/nawshin03/ls6/llvm-project/llvm

```

### 5. Build the specific tools:
```bash
make func-analyzer strength-reducer -j4

```

The tools will appear in:


```bash
/work/11036/nawshin03/ls6/llvm-build/bin/

```
# 4. How to Run the Tools (Lonestar6 Instructions)
GCC include path needed for headers:
```bash
gcc -print-file-name=include
# /opt/apps/gcc/11.2.0/lib/gcc/x86_64-pc-linux-gnu/11.2.0/include

```
Run func-analyzer
```bash
/work/11036/nawshin03/ls6/llvm-build/bin/func-analyzer test.c -- \
  -I/opt/apps/gcc/11.2.0/lib/gcc/x86_64-pc-linux-gnu/11.2.0/include \
  -I/usr/include \
  > func_report.txt
```
Run strength-reducer
```bash
/work/11036/nawshin03/ls6/llvm-build/bin/strength-reducer test.c -- \
  -I/opt/apps/gcc/11.2.0/lib/gcc/x86_64-pc-linux-gnu/11.2.0/include \
  -I/usr/include \
  > transformed_test.c
```
# 5. Repository Contents
```swift
hw2-tools/
 ├── func-analyzer/
 │    ├── FuncAnalyzer.cpp
 │    └── CMakeLists.txt
 ├── strength-reducer/
 │    ├── StrengthReducer.cpp
 │    └── CMakeLists.txt
 ├── example_test.c
 ├── example_func_report.txt
 ├── example_transformed.c (optional)
 └── README.md
```
# 6. Issues & Limitations
### func-analyzer

- Statement counts are based on AST nodes, not visible lines, so they may seem larger.

- Only for, while, do loops are counted—no detection of recursion, switch cases, etc.

- Functions declared but not defined (e.g., external library calls) will appear with zero statements.

### strength-reducer

- Only simple forms (var * power_of_two) are transformed.

- Does not handle complex expressions ((x+3)*8, (x*y)/4, etc.).

- Only integer literals on the right-hand side are supported.

- Does not modify macros or template expansions.

---
# 7. Author

Nawshin Tabassum Tanny

Texas State University

