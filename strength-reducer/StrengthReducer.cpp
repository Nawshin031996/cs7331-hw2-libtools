/*
 *  strength-reducer: A LibTool that performs Operator Strength Reduction.
 *
 *  It scans a C/C++ source file and replaces:
 *    - x * (power of two)  -->  x << k
 *    - x / (power of two)  -->  x >> k
 *
 *  Only simple patterns like "var * const" or "var / const" are handled,
 *  and only in the main source file (not headers).
 */

#include "clang/Basic/SourceManager.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory MyToolCategory("Strength Reduction options");

// ---------------------------------------------------------------------
// Helper: check if value is a power of two and compute shift amount
// ---------------------------------------------------------------------
static bool isPowerOfTwo(unsigned long long val, unsigned &shiftAmount) {
  if (val == 0) return false;
  if ((val & (val - 1)) != 0) return false; // not power of two

  shiftAmount = 0;
  while (val > 1) {
    val >>= 1;
    shiftAmount++;
  }
  return true;
}

// ---------------------------------------------------------------------
// RecursiveASTVisitor: find * and / by powers of two and rewrite them
// ---------------------------------------------------------------------
class StrengthReducerVisitor
  : public RecursiveASTVisitor<StrengthReducerVisitor> {
private:
  ASTContext *astContext;
  Rewriter   &TheRewriter;

public:
  explicit StrengthReducerVisitor(CompilerInstance *CI, Rewriter &R)
      : astContext(&(CI->getASTContext())), TheRewriter(R) {}

  bool VisitBinaryOperator(BinaryOperator *BO) {
    // Only care about * and /
    BinaryOperatorKind Op = BO->getOpcode();
    if (Op != BO_Mul && Op != BO_Div)
      return true;

    SourceManager &SM = astContext->getSourceManager();

    // Only transform expressions in the main source file
    if (SM.getFileID(BO->getExprLoc()) != SM.getMainFileID())
      return true;

    // RHS must be an integer literal
    Expr *RHS = BO->getRHS()->IgnoreParenCasts();
    auto *IntLit = dyn_cast<IntegerLiteral>(RHS);
    if (!IntLit)
      return true;

    unsigned long long val = IntLit->getValue().getLimitedValue();
    unsigned shiftAmount = 0;
    if (!isPowerOfTwo(val, shiftAmount))
      return true;

    // LHS: handle only simple variable expressions (DeclRefExpr)
    Expr *LHS = BO->getLHS()->IgnoreParenCasts();
    auto *LHSRef = dyn_cast<DeclRefExpr>(LHS);
    if (!LHSRef)
      return true;

    std::string lhsName = LHSRef->getNameInfo().getAsString();
    std::string replacement;

    if (Op == BO_Mul) {
      // x * 2^k --> x << k
      replacement = lhsName + " << " + std::to_string(shiftAmount);
    } else { // BO_Div
      // x / 2^k --> x >> k
      replacement = lhsName + " >> " + std::to_string(shiftAmount);
    }

    // Replace the full binary expression with our new text
    TheRewriter.ReplaceText(BO->getSourceRange(), replacement);

    return true;
  }
};

// ---------------------------------------------------------------------
// ASTConsumer: wraps the visitor
// ---------------------------------------------------------------------
class StrengthReducerASTConsumer : public ASTConsumer {
private:
  StrengthReducerVisitor *visitor;

public:
  explicit StrengthReducerASTConsumer(CompilerInstance *CI, Rewriter &R)
      : visitor(new StrengthReducerVisitor(CI, R)) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    SourceManager &SM = Context.getSourceManager();
    auto Decls = Context.getTranslationUnitDecl()->decls();

    // Traverse only decls from the main file
    for (auto &Decl : Decls) {
      const auto &FileID = SM.getFileID(Decl->getLocation());
      if (FileID != SM.getMainFileID())
        continue;
      visitor->TraverseDecl(Decl);
    }
  }
};

// ---------------------------------------------------------------------
// FrontEndAction: sets up Rewriter and prints transformed file
// ---------------------------------------------------------------------
class StrengthReducerFrontendAction : public ASTFrontendAction {
private:
  Rewriter TheRewriter;

public:
  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<StrengthReducerASTConsumer>(&CI, TheRewriter);
  }

  void EndSourceFileAction() override {
    SourceManager &SM = TheRewriter.getSourceMgr();
    FileID mainFileID = SM.getMainFileID();

    const RewriteBuffer *RewriteBuf =
        TheRewriter.getRewriteBufferFor(mainFileID);

    if (RewriteBuf) {
      // Print transformed code
      outs() << std::string(RewriteBuf->begin(), RewriteBuf->end());
    } else {
      // No changes: print original code
      StringRef Buf = SM.getBufferData(mainFileID);
      outs() << Buf;
    }
  }
};

// ---------------------------------------------------------------------
// main: standard LibTooling setup
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
      Tool.run(newFrontendActionFactory<StrengthReducerFrontendAction>().get());

  return result;
}


