
#include <dirent.h>
#include <errno.h>
#include <fstream>
#include <vector>

#include "clang-c/Index.h"
#include "llvm/Support/raw_ostream.h"

// #include "boost/filesystem.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/functional/hash.hpp"
#include "boost/progress.hpp"

#include "config.h"
#include "util.h"


using namespace boost::filesystem;
using namespace std;

// extern tv_string src_dirs;
extern tv_string src_dirs;

size_t uniqueHashString(string str) {
    boost::hash<std::string> string_hash;
    return string_hash(str);
}

void printName2(string file) {
    llvm::outs() << "SRC2:\t\t" << file << "\n";
}

void addSrcFile(tv_string *src_files, string file) {
    llvm::outs() << "addSrcFile:\t\t" << system_complete(path(file)).string() << "\n";
    src_files->push_back(system_complete(path(file)).string());
}

void addSrcDir(tv_string *src_dirs, string dir) {
    llvm::outs() << "addSrcDir:\t" << system_complete(path(dir)).string() << "\n";
    src_dirs->push_back(system_complete(path(dir)).string());
}

void addSrcDirs(tv_string *src_dirs, tv_string src_files) {
    for (t_uint16 i = 0; i < src_files.size(); i++) {
        llvm::outs() << "addSrcDirs:\t" << system_complete(path(src_files[i])).parent_path().string() << "\n";
        src_dirs->push_back(system_complete(path(src_files[i])).parent_path().string().c_str());
    }
}

bool isInRootDirectories(string file, tv_string src_dirs) {
    for (t_uint16 i = 0; i < src_dirs.size(); i++) {
        // llvm::outs() << "Check in directory\t\"" << src_dirs[i] << "\" for file:\t" << file << "\n";
        // if (file.substr(0, strlen(src_dirs[i])).compare(src_dirs[i]) == 0) {
        if (file.substr(0, src_dirs[i].size()).compare(src_dirs[i]) == 0) {
            // llvm::outs() << "File:\t" << file << "\tis in root directories\n";
            return true;
        }
    }
    // llvm::outs() << "File:\t" << file << "\tis not in root directories\n";
    return false;
}

bool isFileParsed(tv_size_t *parsedFiles, size_t newFile) {
    for (t_uint16 i = 0; i < parsedFiles->size(); i++) {
        // string file = (*parsedFiles)[i];
        // llvm::outs() << "compare:\t" << file << "\n";
        if ((*parsedFiles)[i] == newFile) {
            return true;
        }
    }
    // llvm::outs() << "File:\t" << newFile << "\t not found in parsedFiles\n";
    return false;
}

bool isFileParsed(tv_string *parsedFiles, string newFile) {
    for (t_uint16 i = 0; i < parsedFiles->size(); i++) {
        string file = (*parsedFiles)[i];
        // llvm::outs() << "compare:\t" << file << "\n";
        if (file.compare(newFile) == 0) {
            return true;
        }
    }
    // llvm::outs() << "File:\t" << newFile << "\t not found in parsedFiles\n";
    return false;
}

bool isHeader(string file) {
   if ( path(file).extension().compare(".h") == 0 ||
        path(file).extension().compare(".hpp") == 0) {
        // llvm::outs() << "File:\t" << file << "\tis a header file\n";
        return true;
    }
    // llvm::outs() << "File:\t" << file << "\tis not a header file\n";
    return false;
}

bool fileExists(const char *file) {
    ifstream inFile(file);
    return inFile != NULL;
}

bool dirExists(const char *dirName) {
    struct stat sb;

    if (stat(dirName, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        return true;
    }
    return false;
}
// resolves paths, such that ../ and ./ are removed
// allowing the string hash values to be identical
string resolve(string str) {
    return ((resolve(path(str))).string());
}

path resolve(const path& p) {
    path result;
    for(path::iterator it=p.begin(); it!=p.end(); ++it) {
        if(*it == "..") {
            // /a/b/.. is not necessarily /a if b is a symbolic link
            if(boost::filesystem::is_symlink(result) )
                result /= *it;
            // /a/b/../.. is not /a/b/.. under most circumstances
            // We can end up with ..s in our result because of symbolic links
            else if(result.filename() == "..")
                result /= *it;
            // Otherwise it should be safe to resolve the parent
            else
                result = result.parent_path();
        }
        else if (*it == ".") {
            // Ignore
        }
        else {
            // Just cat other path entries
            result /= *it;
        }
    }
    return result;
}


void searchDirectory(path dir, tv_string &source_files, bool recurse) {
    directory_iterator end_iter;
    for (directory_iterator dir_itr(dir); dir_itr != end_iter;
                                                            ++dir_itr ) {
        try {
            if (is_directory(dir_itr->status()) && recurse == true) {
                // std::cout << dir_itr->path().filename() << " [directory]\n";
                searchDirectory(dir_itr->path(), source_files, recurse);
            } else if (is_regular_file(dir_itr->status())) {
                if (dir_itr->path().extension().compare(".cpp") == 0 ||
                    dir_itr->path().extension().compare(".c") == 0) {
                    // llvm::outs() << (dir_itr->path().string().c_str()) << "\n";
                    source_files.push_back(dir_itr->path().string());
                }
            }
        } catch ( const std::exception & ex ) {
            std::cout << dir_itr->path().filename() << " "  << ex.what() << "\n";
        }
    }
}

void getFilesFromDirectories(tv_string src_dirs,
                             tv_string &source_files, bool recurse) {
    for (t_uint16 i = 0; i < src_dirs.size(); i++) {
        // const char *source_dir = src_dirs[i];
        string source_dir = src_dirs[i];
        path full_path( initial_path<path>() );
        full_path = system_complete(path(source_dir));
        searchDirectory(full_path, source_files, recurse);
    }
}

void updateNewNode(t_node *node) {
    // now we need to update the child type count,
    // using the hasing function as the index for each
    // type of node
    t_nodeChildTypeCount *map = &node->typeMap;
    for (t_uint16 i = 0; i < node->children.size(); i++) {
        t_node *child = node->children[i];
        // insert child map into parent map to get hashed values count
        map->insert(child->typeMap.begin(), child->typeMap.end());
        node->numberOfChildren += child->numberOfChildren + 1;
    }
    // then increment count for the new node's hash map
    node->typeMap[node->hashValue]++;
    /*
    // for memory consumption
    if (node->numberOfChildren < MIN_NUM_NODES) {
        // if we do not need the map, clear it!
        map->clear();
    }
    */
}

void getTokensFromCursor(CXCursor cursor, CXTranslationUnit *p_tu,
                        CXToken *tokens, t_uint16 &nTokens) {
    CXSourceRange range = clang_getCursorExtent(cursor);
    clang_tokenize(*p_tu, range, &tokens, &nTokens);
}

string getTokenSpelling(CXToken token, CXTranslationUnit *p_tu) {
    return clang_getCString(clang_getTokenSpelling(*p_tu, token));
}

void freeTokens(CXToken *tokens, t_uint16 nTokens, CXTranslationUnit *p_tu) {
    clang_disposeTokens(*p_tu, tokens, nTokens);
}

// TODO: good hashing algorithm incorporating node's
// children and size?
void hashNode(t_node *node) {
    node->hashValue = (node->kind * CXCursor_EnumEnd);
}

t_uint16 getOperatorValue(string token) {
    if (token.compare("=") == 0) {
        return BO_ASSIGN;
    } else if (token.compare("+") == 0) {
        return BO_ADD;
    } else if (token.compare("-") == 0) {
        return BO_SUB;
    } else if (token.compare("*") == 0) {
        return BO_MUL;
    } else if (token.compare("/") == 0) {
        return BO_DIV;
    } else if (token.compare("%") == 0) {
        return BO_MOD;
    } else if (token.compare("++") == 0) {
        return BO_INCR;
    } else if (token.compare("--") == 0) {
        return BO_DECR;
    } else if (token.compare("==") == 0) {
        return BO_EQUAL;
    } else if (token.compare("!=") == 0) {
        return BO_NEQUAL;
    } else if (token.compare(">") == 0) {
        return BO_GT;
    } else if (token.compare("<") == 0) {
        return BO_LT;
    } else if (token.compare(">=") == 0) {
        return BO_GTEQ;
    } else if (token.compare("<=") == 0) {
        return BO_LTEQ;
    } else if (token.compare("!") == 0) {
        return BO_LNOT;
    } else if (token.compare("&&") == 0) {
        return BO_LAND;
    } else if (token.compare("||") == 0) {
        return BO_LOR;
    } else if (token.compare("~") == 0) {
        return BO_BNOT;
    } else if (token.compare("&") == 0) {
        return BO_BAND;
    } else if (token.compare("|") == 0) {
        return BO_BOR;
    } else if (token.compare("^") == 0) {
        return BO_BXOR;
    } else if (token.compare("<<") == 0) {
        return BO_BLS;
    } else if (token.compare(">>") == 0) {
        return BO_BRS;
    } else {
        return OPERATOR_UNKNOWN;
    }
}

bool isBinaryOperatorCommutative(string token) {
    if (
        token.compare("+") == 0 ||
        token.compare("*") == 0 ||
        token.compare("==") == 0 ||
        token.compare("||") == 0 ||
        token.compare("&&") == 0 ||
        token.compare("~") == 0 ||
        token.compare("&") == 0 ||
        token.compare("|") == 0 ||
        token.compare("^") == 0
    ) {
        return true;
    }
    return false;
}

void printSourceRange(CXTranslationUnit tu, CXSourceRange range) {
    CXToken *tokens = 0;
    unsigned int nTokens = 0;
    clang_tokenize(tu, range, &tokens, &nTokens);
    for (t_uint16 i = 0; i < nTokens; i++) {
        CXString cxString = clang_getTokenSpelling(tu, tokens[i]);
        string token = clang_getCString(cxString);
        llvm::outs() << "token: " << i << "\t\"" << token << "\"\n";
        clang_disposeString(cxString);
    }
    clang_disposeTokens(tu, tokens, nTokens);
}

/*
void printSourceRange(t_node *node) {
    CXSourceRange range = node->loc->srcRange;
    CXTranslationUnit tu = *node->loc->tu;
    CXToken *tokens = 0;
    unsigned int nTokens = 0;
    clang_tokenize(tu, range, &tokens, &nTokens);
    for (t_uint16 i = 0; i < nTokens; i++) {
        CXString cxString = clang_getTokenSpelling(tu, tokens[i]);
        string token = clang_getCString(cxString);
        llvm::outs() << "token: " << i << "\t\"" << token << "\"\n";
        clang_disposeString(cxString);
    }
    clang_disposeTokens(tu, tokens, nTokens);
}
*/

CXChildVisitResult getBinaryOperatorString(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    // string *token = (string*) client_data;
    CXSourceRange range = clang_getCursorExtent(cursor);
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
    CXToken *tokens = 0;
    unsigned int nTokens = 0;
    clang_tokenize(tu, range, &tokens, &nTokens);
    // llvm::outs() << "nTokens == " << nTokens << "\n";
    if (nTokens > 0) {
        CXString cxString = clang_getTokenSpelling(tu, tokens[nTokens - 1]);
        *((string*)client_data) = string(clang_getCString(cxString));
        clang_disposeString(cxString);
    }

    return CXChildVisit_Break;
}

t_uint16 getOperatorHashValue(CXCursor cursor, CXTranslationUnit tu) {
    string token = "";
    CXClientData cData = &token;
    clang_visitChildren(cursor, getBinaryOperatorString, cData);
    /*
    if (cursor.kind == CXCursor_CompoundAssignOperator) {
        llvm::outs() << "token: " << "\t\"" << *((string*)cData) << "\"\n";
        printSourceRange(tu, clang_getCursorExtent(cursor));
    }
    */
    /*
    CXBinaryOpCursor result = CXCursor_NotCommutative;
    if (isBinaryOperatorCommutative(token)) {
        llvm::outs() << "\tBINARY OPERATOR COMMUTATIVE! " << CXCursor_IsCommutative << "\n";
        result = CXCursor_IsCommutative;
    } else {
        llvm::outs() << "\tBINARY OPERATOR NOT COMMUTATIVE!" << CXCursor_NotCommutative << "\n";
        result = CXCursor_NotCommutative;
    }
    return result;
    */
    return getOperatorValue(token);
}

void hashNodeBucketIndex(t_node *node) {
    // TODO some hashing algorithm
    node->hashBucketIndex = (node->kind * CXCursor_EnumEnd) +
                         (node->numberOfChildren / MODULO_NODES);
}

void freeParseTree(t_node *node) {
    if (node == NULL) {
        return;
    }
    for (t_uint16 i = 0; i < node->children.size(); i++) {
        freeParseTree(node->children[i]);
    }
    // free(node);
    delete(node->loc);
    delete(node);
}

t_uint32 hashString(const char *str) {
    int c;
    t_uint32 h = 0;

    while (*str) {
        h += (*str++ % 20);
    }
    // llvm::outs() << "hashString:\t" << h << "\n";
    return h;
}

void hashNode(CXCursor cursor, CXTranslationUnit *p_tu, t_node *node) {
    // llvm::outs() << "hash node\n";
    t_uint32 hashValue;
    hashValue = cursor.kind * CXCursor_EnumEnd;

    // llvm::outs() << "hash node2\n";
    // llvm::outs() << "type:\t" << getCursorKindName(cursor.kind) << "\n";
    switch (cursor.kind) {
        case CXCursor_BinaryOperator: {
            hashValue += getOperatorHashValue(cursor, *p_tu);
            // printSourceRange(node);
            // llvm::outs() << "hash node3\n";
            break;
        }
        case CXCursor_CompoundAssignOperator: {
            
            hashValue += getOperatorHashValue(cursor, *p_tu);
            break;
        }
        /*                                              
        case CXCursor_ObjCInterfaceDecl:
        case CXCursor_ObjCCategoryDecl:
        case CXCursor_ObjCProtocolDecl:
        case CXCursor_ObjCPropertyDecl:
        case CXCursor_ObjCIvarDecl:   
        case CXCursor_ObjCInstanceMethodDecl:
        case CXCursor_ObjCClassMethodDecl:  
        case CXCursor_ObjCImplementationDecl:
        case CXCursor_ObjCCategoryImplDecl: 
        case CXCursor_ObjCSynthesizeDecl:  
        case CXCursor_ObjCDynamicDecl:    
        case CXCursor_ObjCSuperClassRef: 
        case CXCursor_ObjCProtocolRef:  
        case CXCursor_ObjCClassRef:    
        case CXCursor_ObjCMessageExpr:
        case CXCursor_ObjCStringLiteral:
        case CXCursor_ObjCEncodeExpr:  
        case CXCursor_ObjCSelectorExpr:
        case CXCursor_ObjCProtocolExpr:
        case CXCursor_ObjCBridgedCastExpr:
        case CXCursor_ObjCAtTryStmt:
        case CXCursor_ObjCAtCatchStmt:
        case CXCursor_ObjCAtFinallyStmt:
        case CXCursor_ObjCAtThrowStmt:
        case CXCursor_ObjCAtSynchronizedStmt:
        case CXCursor_ObjCAutoreleasePoolStmt:
        case CXCursor_ObjCForCollectionStmt: {
            // for now, do nothing
            break;
        }
        */

        case CXCursor_TypeRef: {
            // then a typedef has been referenced, so
            // hash the type
            CXString cxStr = clang_getCursorDisplayName(cursor);
            // llvm::outs() << "TypeRef:\t" << clang_getTypedefDeclUnderlyingType(cursor).kind << "\n";
            // llvm::outs() << "TypeRef:\t" << string(clang_getCString(cxStr)).size() << "\n";
            hashString(clang_getCString(cxStr));
            hashValue += hashString(clang_getCString(cxStr)); // string(clang_getCString(cxStr)).size();
            clang_disposeString(cxStr);
            break;
        }
        case CXCursor_FunctionDecl:
        case CXCursor_FieldDecl:
        case CXCursor_VarDecl:
        case CXCursor_ParmDecl:/* {
            if (clang_getCursorType(cursor).kind == CXType_Typedef) {
                // llvm::outs() << "Typedef:\t" << clang_getTypedefDeclUnderlyingType(cursor).kind << "\n";
                // if it is a typedef, ignore as the child of this cursor
                // will be a CXCursor_TypeRef
                break;
            }
            hashValue += clang_getCursorType(cursor).kind;
            break;
        }
        */
        case CXCursor_CXXMethod:                          
        case CXCursor_CXXBaseSpecifier:                   
        case CXCursor_DeclRefExpr:                        
        case CXCursor_CallExpr:                           
        case CXCursor_DeclStmt: {
        // TODO
            // llvm::outs() << "Type:\t" << clang_getCursorType(cursor).kind << "\n";
            // then the type is important
            hashValue += clang_getCursorType(cursor).kind;
            break;
        }
/*
        case CXCursor_SwitchStmt
        case CXCursor_UnexposedDecl
        case CXCursor_StructDecl:
        case CXCursor_UnionDecl:
        case CXCursor_ClassDecl:
        case CXCursor_EnumDecl:
        case CXCursor_EnumConstantDecl:
        case CXCursor_TypedefDecl:                        
        case CXCursor_Namespace:                          
        case CXCursor_LinkageSpec:                        
        case CXCursor_Constructor:                        
        case CXCursor_Destructor:                         
        case CXCursor_ConversionFunction:                 
        case CXCursor_TemplateTypeParameter:              
        case CXCursor_NonTypeTemplateParameter:           
        case CXCursor_TemplateTemplateParameter:          
        case CXCursor_FunctionTemplate:                   
        case CXCursor_ClassTemplate:                      
        case CXCursor_ClassTemplatePartialSpecialization: 
        case CXCursor_NamespaceAlias:                     
        case CXCursor_UsingDirective:                     
        case CXCursor_UsingDeclaration:                   
        case CXCursor_TypeAliasDecl:                      
        case CXCursor_CXXAccessSpecifier:                 
        case CXCursor_TemplateRef:                        
        case CXCursor_NamespaceRef:                       
        case CXCursor_MemberRef:                          
        case CXCursor_LabelRef:                           
        case CXCursor_OverloadedDeclRef:                  
        case CXCursor_InvalidFile:                        
        case CXCursor_NoDeclFound:                        
        case CXCursor_NotImplemented:                     
        case CXCursor_MemberRefExpr:                      
        case CXCursor_BlockExpr:                          
        case CXCursor_IntegerLiteral:                     
        case CXCursor_FloatingLiteral:                    
        case CXCursor_ImaginaryLiteral:                   
        case CXCursor_StringLiteral:                      
        case CXCursor_CharacterLiteral:                   
        case CXCursor_ParenExpr:                          
        case CXCursor_UnaryOperator:                      
        case CXCursor_ArraySubscriptExpr:                 
        case CXCursor_ConditionalOperator:                
        case CXCursor_CStyleCastExpr:                     
        case CXCursor_CompoundLiteralExpr:                
        case CXCursor_InitListExpr:                       
        case CXCursor_AddrLabelExpr:                      
        case CXCursor_StmtExpr:                           
        case CXCursor_GenericSelectionExpr:               
        case CXCursor_GNUNullExpr:                        
        case CXCursor_CXXStaticCastExpr:                  
        case CXCursor_CXXDynamicCastExpr:                 
        case CXCursor_CXXReinterpretCastExpr:             
        case CXCursor_CXXConstCastExpr:                   
        case CXCursor_CXXFunctionalCastExpr:              
        case CXCursor_CXXTypeidExpr:                      
        case CXCursor_CXXBoolLiteralExpr:                 
        case CXCursor_CXXNullPtrLiteralExpr:              
        case CXCursor_CXXThisExpr:                        
        case CXCursor_CXXThrowExpr:                       
        case CXCursor_CXXNewExpr:                         
        case CXCursor_CXXDeleteExpr:                      
        case CXCursor_UnaryExpr:                          
        case CXCursor_PackExpansionExpr:                  
        case CXCursor_SizeOfPackExpr:                     
        case CXCursor_UnexposedStmt:                      
        case CXCursor_LabelStmt:                          
        case CXCursor_CaseStmt:                           
        case CXCursor_DefaultStmt:                        
        case CXCursor_IfStmt:                             
        case CXCursor_WhileStmt:                          
        case CXCursor_DoStmt:                             
        case CXCursor_ForStmt:                            
        case CXCursor_GotoStmt:                           
        case CXCursor_IndirectGotoStmt:                   
        case CXCursor_ContinueStmt:                       
        case CXCursor_BreakStmt:                          
        case CXCursor_ReturnStmt:                         
        case CXCursor_AsmStmt:                            
        case CXCursor_CXXCatchStmt:                       
        case CXCursor_CXXTryStmt:                         
        case CXCursor_CXXForRangeStmt:                    
        case CXCursor_SEHTryStmt:                         
        case CXCursor_SEHExceptStmt:                      
        case CXCursor_SEHFinallyStmt:                     
        case CXCursor_NullStmt:                           
        case CXCursor_TranslationUnit:                    
        case CXCursor_UnexposedAttr:                      
        case CXCursor_IBActionAttr:                       
        case CXCursor_IBOutletAttr:                       
        case CXCursor_IBOutletCollectionAttr:             
        case CXCursor_CXXFinalAttr:                       
        case CXCursor_CXXOverrideAttr:                    
        case CXCursor_AnnotateAttr:                       
        case CXCursor_PreprocessingDirective:             
        case CXCursor_MacroDefinition:                    
        case CXCursor_MacroExpansion:                     
        case CXCursor_InclusionDirective:                 
*/
        default: {
            // do nothing
            break;
        }
    }
    

    node->hashValue = hashValue;
    // llvm::outs() << "node hashed\n";
}

/*
t_uint32 hashCursorForBucket(CXCursor cursor) {
    return cursor.kind;
}
*/

t_cursorLocation *createCursorLocation(CXSourceRange *range,
                                        CXTranslationUnit *p_tu, string file) {
    t_cursorLocation *newLoc = new t_cursorLocation();
    CXSourceLocation begin = clang_getRangeStart(*range);
    CXSourceLocation end = clang_getRangeEnd(*range);
    t_uint16 lineStart;
    t_uint16 lineEnd;
    t_uint16 columnBegin;
    t_uint16 columnEnd;

    clang_getExpansionLocation(begin, NULL, &lineStart, &columnBegin, NULL);
    clang_getExpansionLocation(end  , NULL, &lineEnd  , &columnEnd  , NULL);

    newLoc->lineStart = lineStart;
    newLoc->lineEnd = lineEnd;
    newLoc->columnBegin = columnBegin;
    newLoc->columnEnd = columnEnd;
    /*
    newLoc->tu = p_tu;
    // newLoc->srcLoc = clang_getCursorLocation(cursor);
    newLoc->srcRange = clang_getCursorExtent(cursor);
    */
    newLoc->fileName = file;
    return newLoc;
}

t_node *createNode(CXCursor *cursor, CXSourceRange *range,
                                CXTranslationUnit *p_tu, string file) {
    t_node *newNode = new t_node();
    newNode->loc = createCursorLocation(range, p_tu, file);
    newNode->kind = cursor->kind;
    // newNode->hashValue = cursor.kind;// hashCursor(cursor, p_tu);
    // newNode->hashBucketIndex = hashCursorForBucket(cursor);
    newNode->numberOfChildren = 0;
    // newNode->next = NULL;
    return newNode;
}

bool isCursorRelevant(CXCursor cursor) {
    // most kinds are relevant, so only return false for those,
    // and by default, return true
    switch (cursor.kind) {
        case CXCursor_UnexposedAttr: return false;
        case CXCursor_UnexposedDecl: return false;
        case CXCursor_UnexposedExpr: return false;
        case CXCursor_UnexposedStmt: return false;
        default:                     return true;
    };
}

bool isNodeRelevant(t_node *node, CXCursor cursor) {
    switch (node->kind) {
        /*
        case CXCursor_ObjCInterfaceDecl:                  return true;
        case CXCursor_ObjCCategoryDecl:                   return true;
        case CXCursor_ObjCProtocolDecl:                   return true;
        case CXCursor_ObjCPropertyDecl:                   return true;
        case CXCursor_ObjCIvarDecl:                       return true;
        case CXCursor_ObjCInstanceMethodDecl:             return true;
        case CXCursor_ObjCClassMethodDecl:                return true;
        case CXCursor_ObjCImplementationDecl:             return true;
        case CXCursor_ObjCCategoryImplDecl:               return true;
        case CXCursor_ObjCSynthesizeDecl:                 return true;
        case CXCursor_ObjCDynamicDecl:                    return true;
        case CXCursor_ObjCSuperClassRef:                  return true;
        case CXCursor_ObjCProtocolRef:                    return true;
        case CXCursor_ObjCClassRef:                       return true;
        case CXCursor_ObjCMessageExpr:                    return true;
        case CXCursor_ObjCStringLiteral:                  return true;
        case CXCursor_ObjCEncodeExpr:                     return true;
        case CXCursor_ObjCSelectorExpr:                   return true;
        case CXCursor_ObjCProtocolExpr:                   return true;
        case CXCursor_ObjCBridgedCastExpr:                return true;
        case CXCursor_ObjCAtTryStmt:                      return true;
        case CXCursor_ObjCAtCatchStmt:                    return true;
        case CXCursor_ObjCAtFinallyStmt:                  return true;
        case CXCursor_ObjCAtThrowStmt:                    return true;
        case CXCursor_ObjCAtSynchronizedStmt:             return true;
        case CXCursor_ObjCAutoreleasePoolStmt:            return true;
        case CXCursor_ObjCForCollectionStmt:              return true;
        */
        // case CXCursor_UnexposedDecl:                      return true;
        // case CXCursor_StructDecl:                         return true;
        // case CXCursor_UnionDecl:                          return true;
        // case CXCursor_ClassDecl:                          return true;
        // case CXCursor_EnumDecl:                           return true;
        // case CXCursor_FieldDecl:                          return true;
        // case CXCursor_EnumConstantDecl:                   return true;
        case CXCursor_FunctionDecl: {
            if (clang_isCursorDefinition(cursor) != 0) {
                // return true;
            }
            return false;
        }
        // case CXCursor_VarDecl:                            return true;
        // case CXCursor_ParmDecl:                           return true;
        // case CXCursor_TypedefDecl:                        return true;
        case CXCursor_CXXMethod:                          return true;
        // case CXCursor_Namespace:                          return true;
        // case CXCursor_LinkageSpec:                        return true;
        // case CXCursor_Constructor:                        return true;
        // case CXCursor_Destructor:                         return true;
        case CXCursor_ConversionFunction:                 return true;
        // case CXCursor_TemplateTypeParameter:              return true;
        // case CXCursor_NonTypeTemplateParameter:           return true;
        // case CXCursor_TemplateTemplateParameter:          return true;
        // case CXCursor_FunctionTemplate:                   return true;
        // case CXCursor_ClassTemplate:                      return true;
        // case CXCursor_ClassTemplatePartialSpecialization: return true;
        // case CXCursor_NamespaceAlias:                     return true;
        // case CXCursor_UsingDirective:                     return true;
        // case CXCursor_UsingDeclaration:                   return true;
        // case CXCursor_TypeAliasDecl:                      return true;
        // case CXCursor_CXXAccessSpecifier:                 return true;
        // case CXCursor_TypeRef:                            return true;
        // case CXCursor_CXXBaseSpecifier:                   return true;
        // case CXCursor_TemplateRef:                        return true;
        // case CXCursor_NamespaceRef:                       return true;
        // case CXCursor_MemberRef:                          return true;
        // case CXCursor_LabelRef:                           return true;
        // case CXCursor_OverloadedDeclRef:                  return true;
        // case CXCursor_InvalidFile:                        return true;
        // case CXCursor_NoDeclFound:                        return true;
        // case CXCursor_NotImplemented:                     return true;
        // case CXCursor_DeclRefExpr:                        return true;
        // case CXCursor_MemberRefExpr:                      return true;
        // case CXCursor_CallExpr:                           return true;
        // case CXCursor_BlockExpr:                          return true;
        // case CXCursor_IntegerLiteral:                     return true;
        // case CXCursor_FloatingLiteral:                    return true;
        // case CXCursor_ImaginaryLiteral:                   return true;
        // case CXCursor_StringLiteral:                      return true;
        // case CXCursor_CharacterLiteral:                   return true;
        // case CXCursor_ParenExpr:                          return true;
        // case CXCursor_UnaryOperator:                      return true;
        // case CXCursor_ArraySubscriptExpr:                 return true;
        case CXCursor_BinaryOperator: {
            /*
            string token = "";
            CXClientData cData = &token;
            clang_visitChildren(cursor, getBinaryOperatorString, cData);
            // if binary operator is an assignment
            if (token.compare("=") == 0) {
                return true;
            }
            */
            return false;
        }
        case CXCursor_CompoundAssignOperator:             return true;
        case CXCursor_ConditionalOperator:                return true;
        // case CXCursor_CStyleCastExpr:                     return true;
        // case CXCursor_CompoundLiteralExpr:                return true;
        // case CXCursor_InitListExpr:                       return true;
        // case CXCursor_AddrLabelExpr:                      return true;
        // case CXCursor_StmtExpr:                           return true;
        // case CXCursor_GenericSelectionExpr:               return true;
        // case CXCursor_GNUNullExpr:                        return true;
        // case CXCursor_CXXStaticCastExpr:                  return true;
        // case CXCursor_CXXDynamicCastExpr:                 return true;
        // case CXCursor_CXXReinterpretCastExpr:             return true;
        // case CXCursor_CXXConstCastExpr:                   return true;
        // case CXCursor_CXXFunctionalCastExpr:              return true;
        // case CXCursor_CXXTypeidExpr:                      return true;
        // case CXCursor_CXXBoolLiteralExpr:                 return true;
        // case CXCursor_CXXNullPtrLiteralExpr:              return true;
        // case CXCursor_CXXThisExpr:                        return true;
        // only C
        // case CXCursor_CXXThrowExpr:                       return true;
        // case CXCursor_CXXNewExpr:                         return true;
        // case CXCursor_CXXDeleteExpr:                      return true;
        case CXCursor_UnaryExpr:                          return true;
        // case CXCursor_PackExpansionExpr:                  return true;
        // case CXCursor_SizeOfPackExpr:                     return true;
        // case CXCursor_UnexposedStmt:                      return true;
        // case CXCursor_LabelStmt:                          return true;
        case CXCursor_CaseStmt:                           return true;
        case CXCursor_DefaultStmt:                        return true;
        case CXCursor_IfStmt:                             return true;
        case CXCursor_SwitchStmt:                         return true;
        case CXCursor_WhileStmt:                          return true;
        case CXCursor_DoStmt:                             return true;
        case CXCursor_ForStmt:                            return true;
        case CXCursor_GotoStmt:                           return true;
        case CXCursor_IndirectGotoStmt:                   return true;
        // case CXCursor_ContinueStmt:                       return true;
        // case CXCursor_BreakStmt:                          return true;
        // case CXCursor_ReturnStmt:                         return true;
        // case CXCursor_AsmStmt:                            return true;
        // case CXCursor_CXXCatchStmt:                       return true;
        // case CXCursor_CXXTryStmt:                         return true;
        // case CXCursor_CXXForRangeStmt:                    return true;
        // case CXCursor_SEHTryStmt:                         return true;
        // case CXCursor_SEHExceptStmt:                      return true;
        // case CXCursor_SEHFinallyStmt:                     return true;
        // case CXCursor_NullStmt:                           return true;
        // case CXCursor_DeclStmt:                           return true;
        // case CXCursor_TranslationUnit:                    return true;
        // case CXCursor_UnexposedAttr:                      return true;
        // case CXCursor_IBActionAttr:                       return true;
        // case CXCursor_IBOutletAttr:                       return true;
        // case CXCursor_IBOutletCollectionAttr:             return true;
        // case CXCursor_CXXFinalAttr:                       return true;
        // case CXCursor_CXXOverrideAttr:                    return true;
        // case CXCursor_AnnotateAttr:                       return true;
        // case CXCursor_PreprocessingDirective:             return true;
        // case CXCursor_MacroDefinition:                    return true;
        // case CXCursor_MacroExpansion:                     return true;
        // case CXCursor_InclusionDirective:                 return true;
        default:                                          return false;
    }
}

const char* getCursorKindName(t_uint16 c) {
    switch (c) {
        case CXCursor_UnexposedDecl:                      return "CXCursor_UnexposedDecl";
        case CXCursor_StructDecl:                         return "CXCursor_StructDecl";
        case CXCursor_UnionDecl:                          return "CXCursor_UnionDecl";
        case CXCursor_ClassDecl:                          return "CXCursor_ClassDecl";
        case CXCursor_EnumDecl:                           return "CXCursor_EnumDecl";
        case CXCursor_FieldDecl:                          return "CXCursor_FieldDecl";
        case CXCursor_EnumConstantDecl:                   return "CXCursor_EnumConstantDecl";
        case CXCursor_FunctionDecl:                       return "CXCursor_FunctionDecl";
        case CXCursor_VarDecl:                            return "CXCursor_VarDecl";
        case CXCursor_ParmDecl:                           return "CXCursor_ParmDecl";
        case CXCursor_ObjCInterfaceDecl:                  return "CXCursor_ObjCInterfaceDecl";
        case CXCursor_ObjCCategoryDecl:                   return "CXCursor_ObjCCategoryDecl";
        case CXCursor_ObjCProtocolDecl:                   return "CXCursor_ObjCProtocolDecl";
        case CXCursor_ObjCPropertyDecl:                   return "CXCursor_ObjCPropertyDecl";
        case CXCursor_ObjCIvarDecl:                       return "CXCursor_ObjCIvarDecl";
        case CXCursor_ObjCInstanceMethodDecl:             return "CXCursor_ObjCInstanceMethodDecl";
        case CXCursor_ObjCClassMethodDecl:                return "CXCursor_ObjCClassMethodDecl";
        case CXCursor_ObjCImplementationDecl:             return "CXCursor_ObjCImplementationDecl";
        case CXCursor_ObjCCategoryImplDecl:               return "CXCursor_ObjCCategoryImplDecl";
        case CXCursor_TypedefDecl:                        return "CXCursor_TypedefDecl";
        case CXCursor_CXXMethod:                          return "CXCursor_CXXMethod";
        case CXCursor_Namespace:                          return "CXCursor_Namespace";
        case CXCursor_LinkageSpec:                        return "CXCursor_LinkageSpec";
        case CXCursor_Constructor:                        return "CXCursor_Constructor";
        case CXCursor_Destructor:                         return "CXCursor_Destructor";
        case CXCursor_ConversionFunction:                 return "CXCursor_ConversionFunction";
        case CXCursor_TemplateTypeParameter:              return "CXCursor_TemplateTypeParameter";
        case CXCursor_NonTypeTemplateParameter:           return "CXCursor_NonTypeTemplateParameter";
        case CXCursor_TemplateTemplateParameter:          return "CXCursor_TemplateTemplateParameter";
        case CXCursor_FunctionTemplate:                   return "CXCursor_FunctionTemplate";
        case CXCursor_ClassTemplate:                      return "CXCursor_ClassTemplate";
        case CXCursor_ClassTemplatePartialSpecialization: return "CXCursor_ClassTemplatePartialSpecialization";
        case CXCursor_NamespaceAlias:                     return "CXCursor_NamespaceAlias";
        case CXCursor_UsingDirective:                     return "CXCursor_UsingDirective";
        case CXCursor_UsingDeclaration:                   return "CXCursor_UsingDeclaration";
        case CXCursor_TypeAliasDecl:                      return "CXCursor_TypeAliasDecl";
        case CXCursor_ObjCSynthesizeDecl:                 return "CXCursor_ObjCSynthesizeDecl";
        case CXCursor_ObjCDynamicDecl:                    return "CXCursor_ObjCDynamicDecl";
        case CXCursor_CXXAccessSpecifier:                 return "CXCursor_CXXAccessSpecifier";
        case CXCursor_ObjCSuperClassRef:                  return "CXCursor_ObjCSuperClassRef";
        case CXCursor_ObjCProtocolRef:                    return "CXCursor_ObjCProtocolRef";
        case CXCursor_ObjCClassRef:                       return "CXCursor_ObjCClassRef";
        case CXCursor_TypeRef:                            return "CXCursor_TypeRef";
        case CXCursor_CXXBaseSpecifier:                   return "CXCursor_CXXBaseSpecifier";
        case CXCursor_TemplateRef:                        return "CXCursor_TemplateRef";
        case CXCursor_NamespaceRef:                       return "CXCursor_NamespaceRef";
        case CXCursor_MemberRef:                          return "CXCursor_MemberRef";
        case CXCursor_LabelRef:                           return "CXCursor_LabelRef";
        case CXCursor_OverloadedDeclRef:                  return "CXCursor_OverloadedDeclRef";
        case CXCursor_InvalidFile:                        return "CXCursor_InvalidFile";
        case CXCursor_NoDeclFound:                        return "CXCursor_NoDeclFound";
        case CXCursor_NotImplemented:                     return "CXCursor_NotImplemented";
        case CXCursor_InvalidCode:                        return "CXCursor_InvalidCode";
        case CXCursor_UnexposedExpr:                      return "CXCursor_UnexposedExpr";
        case CXCursor_DeclRefExpr:                        return "CXCursor_DeclRefExpr";
        case CXCursor_MemberRefExpr:                      return "CXCursor_MemberRefExpr";
        case CXCursor_CallExpr:                           return "CXCursor_CallExpr";
        case CXCursor_ObjCMessageExpr:                    return "CXCursor_ObjCMessageExpr";
        case CXCursor_BlockExpr:                          return "CXCursor_BlockExpr";
        case CXCursor_IntegerLiteral:                     return "CXCursor_IntegerLiteral";
        case CXCursor_FloatingLiteral:                    return "CXCursor_FloatingLiteral";
        case CXCursor_ImaginaryLiteral:                   return "CXCursor_ImaginaryLiteral";
        case CXCursor_StringLiteral:                      return "CXCursor_StringLiteral";
        case CXCursor_CharacterLiteral:                   return "CXCursor_CharacterLiteral";
        case CXCursor_ParenExpr:                          return "CXCursor_ParenExpr";
        case CXCursor_UnaryOperator:                      return "CXCursor_UnaryOperator";
        case CXCursor_ArraySubscriptExpr:                 return "CXCursor_ArraySubscriptExpr";
        case CXCursor_BinaryOperator:                     return "CXCursor_BinaryOperator";
        case CXCursor_CompoundAssignOperator:             return "CXCursor_CompoundAssignOperator";
        case CXCursor_ConditionalOperator:                return "CXCursor_ConditionalOperator";
        case CXCursor_CStyleCastExpr:                     return "CXCursor_CStyleCastExpr";
        case CXCursor_CompoundLiteralExpr:                return "CXCursor_CompoundLiteralExpr";
        case CXCursor_InitListExpr:                       return "CXCursor_InitListExpr";
        case CXCursor_AddrLabelExpr:                      return "CXCursor_AddrLabelExpr";
        case CXCursor_StmtExpr:                           return "CXCursor_StmtExpr";
        case CXCursor_GenericSelectionExpr:               return "CXCursor_GenericSelectionExpr";
        case CXCursor_GNUNullExpr:                        return "CXCursor_GNUNullExpr";
        case CXCursor_CXXStaticCastExpr:                  return "CXCursor_CXXStaticCastExpr";
        case CXCursor_CXXDynamicCastExpr:                 return "CXCursor_CXXDynamicCastExpr";
        case CXCursor_CXXReinterpretCastExpr:             return "CXCursor_CXXReinterpretCastExpr";
        case CXCursor_CXXConstCastExpr:                   return "CXCursor_CXXConstCastExpr";
        case CXCursor_CXXFunctionalCastExpr:              return "CXCursor_CXXFunctionalCastExpr";
        case CXCursor_CXXTypeidExpr:                      return "CXCursor_CXXTypeidExpr";
        case CXCursor_CXXBoolLiteralExpr:                 return "CXCursor_CXXBoolLiteralExpr";
        case CXCursor_CXXNullPtrLiteralExpr:              return "CXCursor_CXXNullPtrLiteralExpr";
        case CXCursor_CXXThisExpr:                        return "CXCursor_CXXThisExpr";
        case CXCursor_CXXThrowExpr:                       return "CXCursor_CXXThrowExpr";
        case CXCursor_CXXNewExpr:                         return "CXCursor_CXXNewExpr";
        case CXCursor_CXXDeleteExpr:                      return "CXCursor_CXXDeleteExpr";
        case CXCursor_UnaryExpr:                          return "CXCursor_UnaryExpr";
        case CXCursor_ObjCStringLiteral:                  return "CXCursor_ObjCStringLiteral";
        case CXCursor_ObjCEncodeExpr:                     return "CXCursor_ObjCEncodeExpr";
        case CXCursor_ObjCSelectorExpr:                   return "CXCursor_ObjCSelectorExpr";
        case CXCursor_ObjCProtocolExpr:                   return "CXCursor_ObjCProtocolExpr";
        case CXCursor_ObjCBridgedCastExpr:                return "CXCursor_ObjCBridgedCastExpr";
        case CXCursor_PackExpansionExpr:                  return "CXCursor_PackExpansionExpr";
        case CXCursor_SizeOfPackExpr:                     return "CXCursor_SizeOfPackExpr";
        case CXCursor_UnexposedStmt:                      return "CXCursor_UnexposedStmt";
        case CXCursor_LabelStmt:                          return "CXCursor_LabelStmt";
        case CXCursor_CompoundStmt:                       return "CXCursor_CompoundStmt";
        case CXCursor_CaseStmt:                           return "CXCursor_CaseStmt";
        case CXCursor_DefaultStmt:                        return "CXCursor_DefaultStmt";
        case CXCursor_IfStmt:                             return "CXCursor_IfStmt";
        case CXCursor_SwitchStmt:                         return "CXCursor_SwitchStmt";
        case CXCursor_WhileStmt:                          return "CXCursor_WhileStmt";
        case CXCursor_DoStmt:                             return "CXCursor_DoStmt";
        case CXCursor_ForStmt:                            return "CXCursor_ForStmt";
        case CXCursor_GotoStmt:                           return "CXCursor_GotoStmt";
        case CXCursor_IndirectGotoStmt:                   return "CXCursor_IndirectGotoStmt";
        case CXCursor_ContinueStmt:                       return "CXCursor_ContinueStmt";
        case CXCursor_BreakStmt:                          return "CXCursor_BreakStmt";
        case CXCursor_ReturnStmt:                         return "CXCursor_ReturnStmt";
        case CXCursor_AsmStmt:                            return "CXCursor_AsmStmt";
        case CXCursor_ObjCAtTryStmt:                      return "CXCursor_ObjCAtTryStmt";
        case CXCursor_ObjCAtCatchStmt:                    return "CXCursor_ObjCAtCatchStmt";
        case CXCursor_ObjCAtFinallyStmt:                  return "CXCursor_ObjCAtFinallyStmt";
        case CXCursor_ObjCAtThrowStmt:                    return "CXCursor_ObjCAtThrowStmt";
        case CXCursor_ObjCAtSynchronizedStmt:             return "CXCursor_ObjCAtSynchronizedStmt";
        case CXCursor_ObjCAutoreleasePoolStmt:            return "CXCursor_ObjCAutoreleasePoolStmt";
        case CXCursor_ObjCForCollectionStmt:              return "CXCursor_ObjCForCollectionStmt";
        case CXCursor_CXXCatchStmt:                       return "CXCursor_CXXCatchStmt";
        case CXCursor_CXXTryStmt:                         return "CXCursor_CXXTryStmt";
        case CXCursor_CXXForRangeStmt:                    return "CXCursor_CXXForRangeStmt";
        case CXCursor_SEHTryStmt:                         return "CXCursor_SEHTryStmt";
        case CXCursor_SEHExceptStmt:                      return "CXCursor_SEHExceptStmt";
        case CXCursor_SEHFinallyStmt:                     return "CXCursor_SEHFinallyStmt";
        case CXCursor_NullStmt:                           return "CXCursor_NullStmt";
        case CXCursor_DeclStmt:                           return "CXCursor_DeclStmt";
        case CXCursor_TranslationUnit:                    return "CXCursor_TranslationUnit";
        case CXCursor_UnexposedAttr:                      return "CXCursor_UnexposedAttr";
        case CXCursor_IBActionAttr:                       return "CXCursor_IBActionAttr";
        case CXCursor_IBOutletAttr:                       return "CXCursor_IBOutletAttr";
        case CXCursor_IBOutletCollectionAttr:             return "CXCursor_IBOutletCollectionAttr";
        case CXCursor_CXXFinalAttr:                       return "CXCursor_CXXFinalAttr";
        case CXCursor_CXXOverrideAttr:                    return "CXCursor_CXXOverrideAttr";
        case CXCursor_AnnotateAttr:                       return "CXCursor_AnnotateAttr";
        case CXCursor_PreprocessingDirective:             return "CXCursor_PreprocessingDirective";
        case CXCursor_MacroDefinition:                    return "CXCursor_MacroDefinition";
        case CXCursor_MacroExpansion:                     return "CXCursor_MacroExpansion";
        case CXCursor_InclusionDirective:                 return "CXCursor_InclusionDirective";
        default:                                          return "Unknown";
    }
}

