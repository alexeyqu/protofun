// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __PROTOFUN_HXX__
#define __PROTOFUN_HXX__

#include <ostream>
#include <string>
#include <vector>

#include "clang/Driver/Options.h" // me
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ASTContext.h" // me
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/ASTConsumers.h" // me
#include "clang/Frontend/FrontendActions.h" // me
#include "clang/Tooling/CommonOptionsParser.h" // me
#include "clang/Tooling/Tooling.h" // me
#include "clang/Rewrite/Core/Rewriter.h" // me
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"

namespace
{
 /*   class CallBackFunc : public clang::ast_matchers::MatchFinder::MatchCallBack 
    {
      public:
         virtual void run(const clang::ast_matchers::MatcherFinder::MatchResult &Results) {
            auto callee = Results.Nodes.getNodeAs<clang::CallExpr>("callee");
            auto caller = Results.Nodes.getNodeAs<clang::CXXRecordDecl>("caller"); 

           // Do what is required with callee and caller.
        }
    };
*/
    class FunctionsCollector : public clang::RecursiveASTVisitor<FunctionsCollector>
    {
        clang::CompilerInstance & CI;
        const std::string root;
        const std::string lock;
        const std::string out;
        std::vector<std::tuple<std::string, std::string, unsigned>> info;

        std::string CallerFuncName;
        clang::FunctionDecl * CallerFuncDecl;

    public:

        FunctionsCollector(clang::CompilerInstance & __CI, const std::string & __root, const std::string & __lock, const std::string & __out);

        std::pair<std::string, unsigned> getFileLine(const clang::Decl * decl) const;
        std::pair<std::string, unsigned> getFileLine(const clang::Decl * decl, const clang::CallExpr * expr) const;
        void handleFunctionDecl(clang::FunctionDecl * decl);
        void getFunctionName(llvm::raw_string_ostream & out, clang::FunctionDecl * decl);
        bool VisitClassTemplateDecl(clang::ClassTemplateDecl * decl);
        bool VisitFunctionDecl(clang::FunctionDecl * decl);
        bool VisitCallExpr(clang::CallExpr *E);
        void push_info();
        void print_info(std::ostream & os) const;


    private:

        static bool startswith(const std::string & a, const std::string & b);
    };

    class FunctionsCollectorConsumer : public clang::ASTConsumer
    {
        clang::CompilerInstance & CI;
        FunctionsCollector visitor;

    public:

        FunctionsCollectorConsumer(clang::CompilerInstance & __CI, const std::string & root, const std::string & lock, const std::string & out);
        virtual ~FunctionsCollectorConsumer();

        virtual void HandleTranslationUnit(clang::ASTContext & ctxt);
        virtual bool shouldSkipFunctionBody(clang::Decl * decl);
    };


    class FunctionsCollectorAction : public clang::PluginASTAction
    {
        std::string root;
        std::string lock;
        std::string out;

    protected:

        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef) override;
        bool ParseArgs(const clang::CompilerInstance & CI, const std::vector<std::string> & args) override;

        // Automatically run the plugin after the main AST action
        PluginASTAction::ActionType getActionType() override;
    };

}

#endif // __PROTOFUN_HXX__
