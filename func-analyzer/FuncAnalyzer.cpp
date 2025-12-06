/*
 *  LibTool that analyzes functions in a C/C++ source file.
 *
 *  For each function definition in the main source file, it prints:
 *    - Name
 *    - Number of arguments
 *    - Number of statements in the function body
 *    - Number of loops in the function body
 *    - Number of times the function is called in the source file
 */

#include "clang/Basic/SourceManager.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <string>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// Command-line category
static cl::OptionCategory MyToolCategory("Function Analyzer options");

// Info we store per function
struct FunctionInfo {
  std::string Name;
  unsigned NumArgs       = 0;
  unsigned NumStatements = 0;
  unsigned NumLoops      = 0;
  unsigned NumCalls      = 0;
};

// Global table: function name -> info
static std::map<std::string, FunctionInfo> FunctionTable;

// Count how many function *definitions* we saw in the main file
static unsigned NumFunctionDefsInMainFile = 0;

// ---------------------------------------------------------------------
// Helper: recursively count statements and loops in a function body
// ---------------------------------------------------------------------
static void countStmtsAndLoops(Stmt *S,
                               unsigned &stmtCount,
                               unsigned &loopCount) {
  if (!S) return;

  // Count this statement node
  stmtCount++;

  // If this is a loop, increment loop count
  if (isa<ForStmt>(S) || isa<WhileStmt>(S) || isa<DoStmt>(S)) {
    loopCount++;
  }

  // Recurse into children
  for (Stmt *Child : S->children()) {
    if (Child)
      countStmtsAndLoops(Child, stmtCount, loopCount);
  }
}

// ---------------------------------------------------------------------
// Visitor 1: handles function definitions
// ---------------------------------------------------------------------
class FunctionDefVisitor : public RecursiveASTVisitor<FunctionDefVisitor> {
private:
  ASTContext *astContext;

public:
  explicit FunctionDefVisitor(CompilerInstance *CI)
      : astContext(&(CI->getASTContext())) {}

  bool VisitFunctionDecl(FunctionDecl *FD) {
    // Only handle function *definitions* (must have a body)
    if (!FD->hasBody())
      return true;

    SourceManager &SM = astContext->getSourceManager();

    // Only consider functions defined in the main source file
    if (SM.getFileID(FD->getLocation()) != SM.getMainFileID())
      return true;

    NumFunctionDefsInMainFile++;

    std::string funcName = FD->getQualifiedNameAsString();

    FunctionInfo &Info = FunctionTable[funcName];
    Info.Name    = funcName;
    Info.NumArgs = FD->param_size();

    // Count statements and loops in the body
    unsigned stmtCount = 0;
    unsigned loopCount = 0;
    Stmt *Body = FD->getBody();
    countStmtsAndLoops(Body, stmtCount, loopCount);

    Info.NumStatements = stmtCount;
    Info.NumLoops      = loopCount;

    return true;
  }
};

// ---------------------------------------------------------------------
// Visitor 2: handles function calls
// ---------------------------------------------------------------------
class FunctionCallVisitor : public RecursiveASTVisitor<FunctionCallVisitor> {
private:
  ASTContext *astContext;

public:
  explicit FunctionCallVisitor(CompilerInstance *CI)
      : astContext(&(CI->getASTContext())) {}

  bool VisitCallExpr(CallExpr *Call) {
    SourceManager &SM = astContext->getSourceManager();

    // Only count calls in the main source file
    if (SM.getFileID(Call->getExprLoc()) != SM.getMainFileID())
      return true;

    const FunctionDecl *Callee = Call->getDirectCallee();
    if (!Callee)
      return true;

    std::string calleeName = Callee->getQualifiedNameAsString();

    FunctionInfo &Info = FunctionTable[calleeName];
    Info.Name = calleeName;
    Info.NumCalls++;

    return true;
  }
};

// ---------------------------------------------------------------------
// ASTConsumer: wrapper for the two visitors
// ---------------------------------------------------------------------
class FuncAnalysisASTConsumer : public ASTConsumer {
private:
  FunctionDefVisitor  *DefVisitor;
  FunctionCallVisitor *CallVisitor;

public:
  explicit FuncAnalysisASTConsumer(CompilerInstance *CI)
      : DefVisitor(new FunctionDefVisitor(CI)),
        CallVisitor(new FunctionCallVisitor(CI)) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    SourceManager &SM = Context.getSourceManager();
    auto Decls = Context.getTranslationUnitDecl()->decls();

    // Traverse only decls from the main file
    for (auto &Decl : Decls) {
      const auto &FileID = SM.getFileID(Decl->getLocation());
      if (FileID != SM.getMainFileID())
        continue;

      DefVisitor->TraverseDecl(Decl);
      CallVisitor->TraverseDecl(Decl);
    }

    // Print results for each function
    for (auto &Entry : FunctionTable) {
      const FunctionInfo &Info = Entry.second;

      outs() << "Function: " << Info.Name << "\n"
             << "  Number of arguments:   " << Info.NumArgs << "\n"
             << "  Number of statements:  " << Info.NumStatements << "\n"
             << "  Number of loops:       " << Info.NumLoops << "\n"
             << "  Times called in file:  " << Info.NumCalls << "\n\n";
    }

    outs() << "Total function definitions in main file: "
           << NumFunctionDefsInMainFile << "\n";
  }
};

// ---------------------------------------------------------------------
// FrontEndAction: creates ASTConsumer
// ---------------------------------------------------------------------
class FuncAnalysisFrontendAction : public ASTFrontendAction {
public:
  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    return std::make_unique<FuncAnalysisASTConsumer>(&CI);
  }
};

// ---------------------------------------------------------------------
// main: standard LibTooling entry point
// ---------------------------------------------------------------------
int main(int argc, const char **argv) {
  auto ExpectedParser =
      CommonOptionsParser::create(argc, argv, MyToolCategory);

  if (!ExpectedParser) {
    errs() << "Error while parsing command-line arguments\n";
    return 1;
  }

  CommonOptionsParser &op = ExpectedParser.get();
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  int result =
      Tool.run(newFrontendActionFactory<FuncAnalysisFrontendAction>().get());

  return result;
}


