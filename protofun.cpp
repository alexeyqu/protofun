// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include <fstream>
#include <iostream>

#include <sys/file.h>
#include <unistd.h>

#include "protofun.hxx"

namespace
{
    bool FunctionsCollector::startswith(const std::string & a, const std::string & b)
    {
        return b.empty() || (a.length() > b.length() && a.compare(0, b.length(), b) == 0);
    }

    FunctionsCollector::FunctionsCollector(clang::CompilerInstance & __CI, const std::string & __root, const std::string & __lock, const std::string & __out) : CI(__CI), root(__root), lock(__lock), out(__out) { }

    std::pair<std::string, unsigned> FunctionsCollector::getFileLine(const clang::Decl * decl) const
    {        
        if(decl != NULL)
        {
            const clang::SourceLocation loc = decl->getLocation();
            const clang::SourceManager & sm = decl->getASTContext().getSourceManager();
            const clang::FileID id = sm.getFileID(loc);
            const clang::FileEntry * entry = sm.getFileEntryForID(id);
            if (entry)
            {
                const std::string path = entry->tryGetRealPathName();
                if (startswith(path, root))
                {
                    const std::string trunc_path = path.substr(root.length());
                    return std::pair<std::string, unsigned>(trunc_path, sm.getSpellingLineNumber(loc));
                }
                else
                {
                    return std::pair<std::string, unsigned>("", 0);
                }
            }
            else
            {
                return std::pair<std::string, unsigned>("", sm.getSpellingLineNumber(loc));
            }
        }
    }

    std::pair<std::string, unsigned> FunctionsCollector::getFileLine(const clang::Decl * decl, const clang::CallExpr * expr) const
    {
        if(decl != NULL && expr != NULL)
        {
            const clang::SourceLocation loc = expr->getExprLoc();
            const clang::SourceManager & sm = decl->getASTContext().getSourceManager();
            const clang::FileID id = sm.getFileID(loc);
            const clang::FileEntry * entry = sm.getFileEntryForID(id);
            if (entry)
            {
                const std::string path = entry->tryGetRealPathName();
                if (startswith(path, root))
                {
                    const std::string trunc_path = path.substr(root.length());
                    return std::pair<std::string, unsigned>(trunc_path, sm.getSpellingLineNumber(loc));
                }
                else
                {
                    return std::pair<std::string, unsigned>("", 0);
                }
            }
            else
            {
                return std::pair<std::string, unsigned>("", sm.getSpellingLineNumber(loc));
            }
        }
    }


    void FunctionsCollector::getFunctionName(llvm::raw_string_ostream & out, clang::FunctionDecl * decl)
    {        
        if(decl != NULL)
        {
            const clang::PrintingPolicy & policy = CI.getASTContext().getPrintingPolicy();

            decl->getNameForDiagnostic(out, policy, true);
            out << '(';
            bool first = true;
            for (auto && parameter : decl->parameters())
            {
                if (!first)
                {
                    out << ", ";
                }
                else
                {
                    first = false;
                }
                out << parameter->getOriginalType().getAsString(policy);
            }
            out << ')';
        }
    }

    void FunctionsCollector::handleFunctionDecl(clang::FunctionDecl * decl)
    {        
        if(decl != NULL)
        {
            if (decl->isThisDeclarationADefinition() && !decl->isDeleted())
            {
                clang::FunctionDecl * fdecl = decl->getDefinition();
                if (clang::FunctionTemplateDecl * ftdecl = fdecl->getDescribedFunctionTemplate())
                {
                    for (auto && specialization : ftdecl->specializations())
                    {
                        handleFunctionDecl(specialization);
                    }
                }
                else
                {
                    std::string s;
                    llvm::raw_string_ostream out(s);

                    getFunctionName(out, decl);

                    const auto fn = getFileLine(decl);
                    if (!fn.first.empty() && fn.second)
                    {
                       info.emplace_back(std::make_tuple(DotEntryType::DOT_DECL, std::string("declaration"),  out.str(), fn.first, fn.second));
                    }
                }
            }
        }
    }

    bool FunctionsCollector::VisitClassTemplateDecl(clang::ClassTemplateDecl * decl)
    {
        if(decl != NULL)
        {
            for (auto && specialization : decl->specializations())
            {
                if (!clang::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(specialization))
                {
                    for (auto && method : specialization->methods())
                    {
                        handleFunctionDecl(method);
                    }
                }
            }
        }
        return true;
    }

    bool FunctionsCollector::VisitFunctionDecl(clang::FunctionDecl * decl)
    {   
        if(decl != NULL)
        {    
            std::string newFuncDecl = "";
            llvm::raw_string_ostream callerStream(newFuncDecl);      
            getFunctionName(callerStream, decl);
            CallerFuncName = callerStream.str();

            CallerFuncDecl = decl;

            if (clang::CXXMethodDecl * cmdecl = clang::dyn_cast<clang::CXXMethodDecl>(decl))
            {
                if (clang::CXXRecordDecl * parent = cmdecl->getParent())
                {
                    if (!parent->getDescribedClassTemplate() && !clang::dyn_cast<clang::ClassTemplateSpecializationDecl>(parent))
                    {
                        handleFunctionDecl(decl);
                    }
                }
            }
            else
            {
                handleFunctionDecl(decl);
            }
        }
        return true;
    }

    bool FunctionsCollector::VisitCallExpr(clang::CallExpr * expr) 
    {       
        if (expr != NULL)
        {
            clang::QualType qualtype = expr->getType();
            const clang::Type * type = qualtype.getTypePtrOrNull();

            if(type != NULL)
            {
                clang::FunctionDecl * decl = expr->getDirectCallee();

                if(decl)
                {
                    std::string s;
                    llvm::raw_string_ostream out(s);
                    getFunctionName(out, decl);

                    const auto fn = getFileLine(CallerFuncDecl, expr);
                    if (!fn.first.empty() && fn.second)
                    {
                       info.emplace_back(std::make_tuple(DotEntryType::DOT_CALL, CallerFuncName, out.str(), fn.first, fn.second));
                    }
                }
                else if(clang::dyn_cast< clang::CXXDependentScopeMemberExpr >(expr->getCallee()))
                {
                    std::string s;
                    llvm::raw_string_ostream out(s);
                    
                    out << clang::dyn_cast< clang::CXXDependentScopeMemberExpr >(expr->getCallee())->getMember().getAsString(); // yup, cheating

                    const auto fn = getFileLine(CallerFuncDecl, expr);
                    if (!fn.first.empty() && fn.second)
                    {
                       info.emplace_back(std::make_tuple(DotEntryType::DOT_CALL, CallerFuncName, out.str(), fn.first, fn.second));
                    }
                }
                else
                {
                    // here gotta be some other Expr variants
                    std::cout << "Unhandled expression..yet." << std::endl;
                }
            }
        }
        return true;
    }

    void FunctionsCollector::print_info(std::ostream & os) const
    {
        os << "digraph callgraph {\n";
        for (auto && i : info)
        {
            switch(std::get<0>(i))
            {
                case DotEntryType::DOT_DECL:
                    os << "\"" << std::get<2>(i) << "\";\n";
                    break;

                case DotEntryType::DOT_CALL:
                    os << "\"" << std::get<1>(i) << "\"" 
                       << "->" << "\"" << std::get<2>(i) 
                       << "\" [label = \"" << std::get<3>(i) << ":" 
                       << std::get<4>(i) << "\"];\n";
                    break;

                default:
                    exit(1);
            }
        }
        os << "}\n";

        os << std::flush;
    }

    void FunctionsCollector::push_info()
    {
        if (out.empty())
        {
            print_info(std::cout);
        }
        else if (lock.empty())
        {
            std::ofstream os(out, std::ios_base::app);
            print_info(os);
            os.close();
        }
        else
        {
            const int fd = open(lock.c_str(), O_RDONLY);
            const int s = flock(fd, LOCK_EX);
            if (s == 0)
            {
                std::ofstream os(out, std::ios_base::app);
                print_info(os);
                os.close();

                flock(fd, LOCK_UN);
                close(fd);
            }
        }
        info.clear();
    }

    FunctionsCollectorConsumer::FunctionsCollectorConsumer(clang::CompilerInstance & __CI, const std::string & root, const std::string & lock, const std::string & out) : clang::ASTConsumer(), CI(__CI), visitor(__CI, root, lock, out) { }

    FunctionsCollectorConsumer::~FunctionsCollectorConsumer() { }

    void FunctionsCollectorConsumer::HandleTranslationUnit(clang::ASTContext & ctxt)
    {
        visitor.TraverseDecl(ctxt.getTranslationUnitDecl());
        visitor.push_info();
    }

    bool FunctionsCollectorConsumer::shouldSkipFunctionBody(clang::Decl * decl)
    {
        return true;
    }

    std::unique_ptr<clang::ASTConsumer> FunctionsCollectorAction::CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef)
    {
        return llvm::make_unique<FunctionsCollectorConsumer>(CI, root, lock, out);
    }

    bool FunctionsCollectorAction::ParseArgs(const clang::CompilerInstance & CI, const std::vector<std::string> & args)
    {
        if (args.size() >= 1)
        {
            root = args[0];
            if (args.size() >= 2)
            {
                out = args[1];
                if (args.size() >= 3)
                {        
                    lock = args[2];
                }
            }
        }

        return true;
    }

    // Automatically run the plugin after the main AST action
    clang::PluginASTAction::ActionType FunctionsCollectorAction::getActionType()
    {
        return AddAfterMainAction;
    }

    static clang::FrontendPluginRegistry::Add<FunctionsCollectorAction> X("protofun", "get function prototypes");
}
