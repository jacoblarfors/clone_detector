
#include <algorithm>
#include <dirent.h>
#include <errno.h>
#include <fstream>
#include <getopt.h>
#include <map>
#include <sys/time.h>
#include <vector>

#include "llvm/Support/raw_ostream.h"
#include "HTMLGenerator.h"
#include "util.h"

using namespace std;

// tvp_char src_dirs;
tv_string src_dirs;

// similarity function taken from Baxter et al.'s algorithm
double similarityFunction(double S, double L, double R) {
    return (2 * S) /
        ((2 * S) + L + R);
}

bool checkSimilarity(t_node *node_1, t_node *node_2) {
    double similarity;
    double S = 0, L = 0, R = 0;
    t_uint16 count_1 = 0, count_2 = 0, roundSimilarity;
    // QUESTION: creates copy, can we reference and remove?
    t_nodeChildTypeCount typeMap_1 = node_1->typeMap;
    t_nodeChildTypeCount typeMap_2 = node_2->typeMap;

    /*
    llvm::outs() << "COMPARE TWO NODES:\n";
    printSourceRange(node_1);
    llvm::outs() << "\n";
    printSourceRange(node_2);
    llvm::outs() << "\n";
    */

    t_nodeChildTypeCount::iterator map_it = typeMap_1.begin();
    while (map_it != typeMap_1.end()) {
        count_1 = map_it->second;
        count_2 = typeMap_2[map_it->first]; //.second;
        // update S, L, R
        roundSimilarity = MIN(count_1, count_2);
        S = S + roundSimilarity;
        L = L + (count_1 - roundSimilarity);
        R = R + (count_2 - roundSimilarity);
        typeMap_2.erase(map_it->first);
        typeMap_1.erase(map_it++);
    }
    map_it = typeMap_2.begin();
    while (map_it != typeMap_2.end()) {
        count_2 = map_it->second;
        L += count_2;
        typeMap_2.erase(map_it++);
    }
    /*
    llvm::outs() << "S = " << S << "\n";
    llvm::outs() << "L = " << L << "\n";
    llvm::outs() << "R = " << R << "\n";
    */
    
    similarity = similarityFunction(S, L, R);
    // llvm::outs() << "################## SIMILARITY ======= " << similarity << "\n";
    if (similarity > SIMILARITY_THRESHOLD) {
        return true;
    } else {
        return false;
    }
}

void bucketHashNode(t_node *node, t_map *hash_map) {
    if (node->numberOfChildren >= MIN_NUM_NODES) {
        // then place the node into its hash bucket
        t_hashBucket *bucket = &((*hash_map)[node->hashBucketIndex]);
        bucket->push_back(node);
    }
}

void printName(string file) {
    llvm::outs() << "SRC:\t\t" << file << "\n";
}

CXChildVisitResult traverseAST(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    if (!isCursorRelevant(cursor)) {
        return CXChildVisit_Recurse;
    }
    // retrieve parser information
    t_hashMapParserInfo *parserInfo = ((t_hashMapParserInfo*) client_data);

    // t_parseSourceFile   *parentFile = &parserInfo->parentFile;
    CXSourceRange range             = clang_getCursorExtent(cursor);
    CXSourceLocation newCursorLoc   = clang_getRangeStart(range); //clang_getCursorLocation(cursor);
    CXFile cxFileNew;

    // llvm::outs() << clang_getCString(clang_getCursorKindSpelling(cursor.kind)) << "\n";
    CXString cxStrFileNew;
    const char *fileNew;
    clang_getFileLocation(newCursorLoc, &cxFileNew, NULL, 0, NULL);
    cxStrFileNew = clang_getFileName(cxFileNew);
    fileNew = clang_getCString(cxStrFileNew);

    string strFile;
    size_t strFileHash;
    
    if (fileNew != NULL) {
        strFile = resolve(string(fileNew));
        strFileHash = uniqueHashString(strFile);
        // if the current file is not the same as the parent file
        // if (strFile.compare(*parserInfo->parentFile->fileName) != 0) {
        if (strFileHash != parserInfo->parentFile->fileName) {
            // llvm::outs() << "MEH!:tu_file=" << *parserInfo->tu_file << "\t\tstrFile=" << strFile << "\n";
            // *parserInfo->isInclusion = true;
            parserInfo->parentFile->fileName = strFileHash;
            parserInfo->parentFile->skipFile = false;

            // then we want to check the new file
            tv_size_t *fin_parsedFiles = &parserInfo->fin_parsed_src_files;
            tv_size_t *tu_parsedFiles = &parserInfo->tu_parsed_src_files;
            if (((parserInfo->tu_file != strFileHash) && 
                    (!isInRootDirectories(strFile, src_dirs)))){ // ||
                // llvm::outs() << "SKIPPING FILE_0:\t\t" << strFile << "\n";
                parserInfo->parentFile->skipFile = true;
                return CXChildVisit_Continue;
            } if((isFileParsed(fin_parsedFiles, strFileHash))) {
                // llvm::outs() << "SKIPPING FILE_1:\t\t" << strFile << "\n";
                parserInfo->parentFile->skipFile = true;
                return CXChildVisit_Continue;
            }
            if (!isFileParsed(tu_parsedFiles, strFileHash)) {
                // llvm::outs() << "NEW SOURCE FILE:::::::\t" << fileNew << "\n";
                tu_parsedFiles->push_back(strFileHash);
                // llvm::outs() << "NEW FILE:\t\t" << resolve(strFile) << "\n";
            }

        } else if (parserInfo->parentFile->skipFile == true) {
            // llvm::outs() << "SAME FILE, SKIPPING:\t" << strFile << "\n";
            return CXChildVisit_Continue;
        }
        // llvm::outs() << "NODE IN FILE:\t" << *strFile << "\n";
    }

    // retrieve the parent node
    t_node *parentNode = parserInfo->parent;
    t_uint16 depth = parserInfo->depth;
    // create the new node
    t_node *newNode = createNode(&cursor, &range, parserInfo->tu, strFile);
    // add new node to the parent
    parentNode->children.push_back(newNode);

    /*
    // START DEBUG
    if (fileNew != NULL) {
        for (t_uint16 i = 0; i < depth; i++ ) {
            llvm::outs() << "  ";
        }
        llvm::outs() << getCursorKindName(cursor.kind) << ":" << clang_getCString(clang_getCursorDisplayName(cursor)) << "\n";
        // llvm::outs() << *strFile << "\n";
    }
    // llvm::outs() << "HASH:\t" << newNode->hashValue << "\n";
    depth++;
    // END DEBUG
    */

    // initialise the parser information for this cursor's children
    t_hashMapParserInfo newParserInfo = {
        parserInfo->tu_file, // parserInfo->isInclusion,
        // parserInfo->parentFile, depth,
        parserInfo->parentFile, depth,
        parserInfo->tu, parserInfo->hash_map, newNode,
        parserInfo->tu_parsed_src_files, parserInfo->fin_parsed_src_files
    };

    CXClientData data = &newParserInfo;

    hashNode(cursor, parserInfo->tu, newNode);
    clang_visitChildren(cursor, traverseAST, data);

    updateNewNode(newNode);
    hashNodeBucketIndex(newNode);
    
    if (isNodeRelevant(newNode, cursor)) {
        bucketHashNode(newNode, parserInfo->hash_map);
    }
    // clang_disposeString(cxStrFileNew);
    // clang_disposeString(cxStrFileParent);

    return CXChildVisit_Continue;
}

void printTreeStructure(t_node *root, t_uint16 depth) {
    if (root == NULL) {
        // llvm::outs() << "NODE IS NULL!\n";
        return;
    }
    /*
    for (t_uint16 i = 0; i < depth; i++ ) {
        llvm::outs() << "  ";
    }
    llvm::outs() << getCursorKindName(root->kind) << "\t" << root->kind << "\t" << root->numberOfChildren << "\n";

    for (t_nodeChildTypeCount::iterator iter = root->typeMap.begin(); iter != root->typeMap.end(); iter++) {
        for (t_uint16 i = 0; i < depth; i++ ) {
            llvm::outs() << "  ";
        }
        // llvm::outs() << "Key: " << iter->first << "\tCount: " << iter->second << "\n";
    }
    */
    
    for (t_uint16 i = 0; i < root->children.size(); i++) {
        printTreeStructure(root->children[i], depth + 1);
    }

    // printTreeStructure(root->next, depth);
}

void compareBucketSubtrees(t_map &hash_map, t_clones &clones) {
    // llvm::outs() << "\n\nhash_map size = " << hash_map.size() << "\n";
    t_clonePair *clonePair;
    t_map::iterator map_iterator;
    for (map_iterator = hash_map.begin(); map_iterator != hash_map.end(); ++map_iterator) {
        // t_cursorLocations locs = map_iterator->second;
        // llvm::outs() << "BUCKET CURSOR KIND = " << getCursorKindName(map_iterator->first) << "\n";
        t_hashBucket hash_bucket = map_iterator->second;
        llvm::outs() << "BUCKET SIZE               =\t" << hash_bucket.size() << "\n";
        if (hash_bucket.size() > 0) {
            llvm::outs() << "BUCKET CURSOR KIND         =\t" << getCursorKindName(hash_bucket[0]->kind) << "\n";
            // llvm::outs() << "NUM_CHILDREN % MODULO_NODES=\t" << (hash_bucket[0]->numberOfChildren / MODULO_NODES) << "\n\n";
        }
        if (hash_bucket.size() < 2) {
            // then remove as cannot have code clone with only 1 occurence
            // hash_map.erase(map_iterator);
        } else {
            t_hashBucket::iterator it_bucketCandidate;
            t_hashBucket::iterator it_bucketComparator;
            // else check clone candidates to see if they actually are clones
            for (it_bucketCandidate = hash_bucket.begin();
                    it_bucketCandidate != hash_bucket.end(); it_bucketCandidate++) {
                // get the translation unit
                // CXTranslationUnit tu = *(*it_bucketCandidate)->loc->tu;
                // llvm::outs() << "SRC = " << clang_getCString(clang_getTranslationUnitSpelling(tu)) << "\n";
                // retrieve the cursor for this node in the translation unit
                // CXCursor cursor = clang_getCursor(tu,
                //            (*it_bucketCandidate)->loc->srcLoc);
                // now compare against other entries in same bucket
                for (it_bucketComparator = it_bucketCandidate + 1;
                        it_bucketComparator != hash_bucket.end(); it_bucketComparator++) {
                    // CXTranslationUnit comp_tu = *(*it_bucketComparator)->loc->tu;
                    // CXCursor comp_cursor = clang_getCursor(comp_tu,
                    //    (*it_bucketComparator)->loc->srcLoc);
                    // check similarity of clone candidates
                    // llvm::outs() << "Compare " << clang_getCString(clang_getTranslationUnitSpelling(*(*it_bucketCandidate)->loc->tu)) << "\n";
                    // llvm::outs() << "and     " << clang_getCString(clang_getTranslationUnitSpelling(*(*it_bucketComparator)->loc->tu)) << "\n";
                    // if (checkSimilarity(*it_bucketCandidate, *it_bucketComparator) == true) {
                    if (checkSimilarity(*it_bucketCandidate, *it_bucketComparator) == true) {
/*
                        llvm::outs() << "WE HAVE A CLONE MATCH!\n";
                        llvm::outs() << clang_getCString(clang_getTranslationUnitSpelling(*(*it_bucketCandidate)->loc->tu)) << "\n";
                        llvm::outs() << clang_getCString(clang_getTranslationUnitSpelling(*(*it_bucketComparator)->loc->tu)) << "\n";
*/
                        // then store clone pair
                        clonePair = new t_clonePair();
                        clonePair->first = *it_bucketCandidate;
                        clonePair->second = *it_bucketComparator;
                        clones.push_back(clonePair);
                        // hashLocations = &(*parserInfo->hash_map)[cursor.kind];
                        // insert new location
                        // hashLocations->push_back(cursorInfo);
                    }
                }
            }
        }
    }
}

void parseCommandLineArgs(int argc, char **argv, tv_string *recurse_src_dirs,
                        tv_string *src_files, bool *runDiagnostics,
                        tv_string *includes) {
    extern char *optarg;
    char c;
    int longIndex;
    static struct option longOptions[] = {
        {"source", required_argument, NULL, 's'},
        {"directory", required_argument, NULL, 'd'},
        {"recurse_directory", required_argument, NULL, 'r'},
        {"diagnostics", no_argument, NULL, 'a'},
        {"include", required_argument, NULL, 'i'}
    };
    while (-1 != (c = getopt_long(argc, argv, "s:d:r:ai:l:D:",
                                    longOptions, &longIndex))) {
        switch (c) {
            case 0: {
                break;
            }
            case 's' : {
                if (!fileExists(optarg)) {
                    llvm::outs() << "Could not locate source file \"" << optarg
                                 << "\". Exiting.\n";
                    exit(1);
                }
                addSrcFile(src_files, string(optarg)); 
                break;
            }
            case 'd' : {
                if (!dirExists(optarg)) {
                    llvm::outs() << "Could not locate directory \"" << optarg
                                 << "\". Exiting.\n";
                    exit(1);
                }
                addSrcDir(&src_dirs, string(optarg)); 
                break;
            }
            case 'r' : {
                if (!dirExists(optarg)) {
                    llvm::outs() << "Could not locate directory \"" << optarg
                                 << "\". Exiting.\n";
                    exit(1);
                }
                addSrcDir(recurse_src_dirs, string(optarg)); 
                break;
            }
            case 'a' : {
                *runDiagnostics = true;
                break;
            }
            case 'i' : {
                includes->push_back(string("-I") + string(optarg));
                break;
            }
            case 'l' : {
                includes->push_back(string("-l") + string(optarg));
                break;
            }
            case 'D' : {
                includes->push_back(string("-D") + string(optarg));
                break;
            }
            default : {
                // llvm::outs() << "HERE!\n";
                break;
            }
        }
    }
}

void printParsedFiles(string file) {
    llvm::outs() << "\t- " << file << "\n";
}

int main(int argc, char **argv) {
    tv_string recurse_src_dirs;
    tv_string src_files;
    tv_string arg_src_files;
    tv_string includes;
    vector<t_node*> rootNodes;
    bool runDiagnostics = false;
    struct timeval tv_Start, tv_End;
    double elapsedTime;

    parseCommandLineArgs(argc, argv, &recurse_src_dirs, &arg_src_files,
                                            &runDiagnostics, &includes);

    if (src_dirs.size() > 0) {
        getFilesFromDirectories(src_dirs, src_files, false);
    }
    // retrieve source files from the directories given as recursive
    // (passed via -r <directory>)
    if (recurse_src_dirs.size() > 0) {
        getFilesFromDirectories(recurse_src_dirs, src_files, true);
        // put directories to recurse into list of source directories
        // to use for distinguishing between headers and to parse and skip
        addSrcDirs(&src_dirs, recurse_src_dirs);
    }

    // put source file's (passed via -s <argument>) parent directories
    // into the list of src_dirs to distinguish between which headers
    // to parse and which to skip
    addSrcDirs(&src_dirs, arg_src_files);

    src_files.insert(src_files.end(), arg_src_files.begin(), arg_src_files.end());
    // for_each(src_files.begin(), src_files.end(), printName);
    // for_each(src_dirs.begin(), src_dirs.end(), printName);
    for_each(src_files.begin(), src_files.end(), printName);
    llvm::outs() << "SRC_FILES SIZE == \t\t" << src_files.size() << "\n";
    for_each(src_dirs.begin(), src_dirs.end(), printName);
    llvm::outs() << "SRC_DIRS SIZE == \t\t" << src_dirs.size() << "\n";

    gettimeofday(&tv_Start, NULL);

    // intialise the hash table to use
    t_map hash_map;
    t_clones clones;
    tv_size_t parsedSourceFiles;
    // tv_string finParsedSourceFiles;
    tv_size_t finParsedSourceFiles;
    string strDiagnosis;
    t_uint16 totalDiagnostics = 0;
    t_uint16 totalSevereDiagnostics = 0;
    const char **newArgv = new const char *[2 + includes.size()];
    t_uint16 newArgc = 2 + includes.size();
    for (t_uint16 i = 0; i < includes.size(); i++) {
        newArgv[i + 2] = includes[i].c_str();
    }

    CXIndex index = clang_createIndex(0,0);
    CXTranslationUnit tu;
    for (t_uint16 i = 0; i < src_files.size(); i++) {
        llvm::outs() << "Parsing source file " << src_files[i].c_str() << "...\n";
        // llvm::outs() << src_files[i] << "\n";
        // setup the new arguments for the translation unit
        newArgv[0] = argv[0];
        newArgv[1] = src_files[i].c_str();
        llvm::outs() << "newArgv = ";
        for (t_uint16 j = 0; j < newArgc; j++) {
            llvm::outs() << newArgv[j] << " ";
        }
        llvm::outs() << "\n";
        // initialise the translation unit
        tu = clang_parseTranslationUnit(index, 0,
            newArgv, newArgc, 0, 0, CXTranslationUnit_None);
        if (tu == NULL) {
            break;
        }
        // get the root cursor for the translation unit
        CXCursor rootCursor = clang_getTranslationUnitCursor(tu);
        // t_uint16 *depth = (t_uint16*)malloc(sizeof(t_uint16)); ;
        // *depth = 0;
        t_uint16 depth = 0;
        t_node *root = createNode(
                &rootCursor, &clang_getCursorExtent(rootCursor),
                &tu, src_files[i]
        );
        llvm::outs() << "parsed source files size::::\t" << finParsedSourceFiles.size() << "\n";
        t_parentFile parentFile = {
            uniqueHashString(src_files[i]), false
        };
        // initialise the data to pass to traverseAST function
        t_hashMapParserInfo parserInfo = {
            uniqueHashString(src_files[i]), &parentFile, depth, &tu, &hash_map, root,
            parsedSourceFiles, finParsedSourceFiles
        };
        t_uint16 diagnostics = 0;
        t_uint16 severeDiagnostics = 0;
        // if run diagnostics...then
        if (runDiagnostics == true) {
            llvm::outs() << "Running diagnostics on source file " <<
                                         src_files[i].c_str() << "...\n";
            for (t_uint16 j = 0, diagnostics = clang_getNumDiagnostics(
                                    tu); j != diagnostics; ++j) {
                CXDiagnostic Diag = clang_getDiagnostic(tu, j);
                if (clang_getDiagnosticSeverity(Diag) > CXDiagnostic_Warning) {
                    llvm::outs() << "SEVERE_DIAGNOSTIC!:\n";
                    severeDiagnostics++;
                }
                CXString cxStr = clang_formatDiagnostic(Diag, 
                        clang_defaultDiagnosticDisplayOptions());
                llvm::outs() << clang_getCString(cxStr) << "\n";
                strDiagnosis.append(clang_getCString(cxStr)).append("\n");
                clang_disposeString(cxStr);
            }
        }
        llvm::outs() << "diagnostics:\t" << diagnostics << "\n";
        if (severeDiagnostics == 0) {
            // set the client data in traverseAST
            CXClientData data = &parserInfo;
            clang_visitChildren(rootCursor, traverseAST, data);
            llvm::outs() << "Updating tree...\n";
            updateNewNode(root);
            // llvm::outs() << "\n\n";
            // printTreeStructure(root, 0);
            rootNodes.push_back(root);
            llvm::outs() << "Source file "/* << src_files[i].c_str() << " */"parsed!\n\n";
            finParsedSourceFiles.insert(
                finParsedSourceFiles.end(),
                parserInfo.tu_parsed_src_files.begin(),
                parserInfo.tu_parsed_src_files.end());
        } else {
            llvm::outs() << "Skipping file due to diagnostics!\n\n";
        }
        totalSevereDiagnostics += severeDiagnostics;
        totalDiagnostics += diagnostics;
        clang_disposeTranslationUnit(tu);
    }
    clang_disposeIndex(index);
    free(newArgv);

    // llvm::outs() << "\n\nSource files parsed:\n";
    // for_each(finParsedSourceFiles.begin(), finParsedSourceFiles.end(), printParsedFiles);

    // now traverse through hash map and compare similar subtrees!
    compareBucketSubtrees(hash_map, clones);
    llvm::outs() << "\n\nnumber of clones found = " << clones.size() << "\n";

    gettimeofday(&tv_End, NULL);

    elapsedTime = (1000000.0 *
                    (tv_End.tv_sec - tv_Start.tv_sec) +
                    tv_End.tv_usec - tv_Start.tv_usec);

    // if we have found any clones, create results
    if (clones.size() > 0) {
        HTMLGenerator htmlGen("html", clones);
        htmlGen.generateHTMLCode();        
    }

    // llvm::outs() << "rootNodes.size() == " << rootNodes.size() << "\n";
    for (t_uint16 i = 0; i < rootNodes.size(); i++) {
        // llvm::outs() << "free root: " << i << "\n";
        freeParseTree(rootNodes[i]);
    }
    for (t_uint16 i = 0; i < clones.size(); i++) {
        delete(clones[i]);
    }
    llvm::outs() << "\nDIAGNOSTICS:\n";
    llvm::outs() << "TOTAL:\t\t" << totalDiagnostics << "\t\tSEVERE:\t\t" << totalSevereDiagnostics << "\n";
    // llvm::outs () << "\n" << strDiagnosis.c_str() << "\n";
    printf("\nELAPSED_TIME = %f\n", elapsedTime);
    return 0;
}

