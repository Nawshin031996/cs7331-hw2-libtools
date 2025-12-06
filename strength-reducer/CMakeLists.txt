set(LLVM_LINK_COMPONENTS support)

add_clang_executable(strength-reducer
  StrengthReducer.cpp
)

target_link_libraries(strength-reducer
  PRIVATE
  clangTooling
  clangBasic
  clangASTMatchers
)


