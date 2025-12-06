set(LLVM_LINK_COMPONENTS support)

add_clang_executable(func-analyzer
  FuncAnalyzer.cpp
)

target_link_libraries(func-analyzer
  PRIVATE
  clangTooling
  clangBasic
  clangASTMatchers
)

