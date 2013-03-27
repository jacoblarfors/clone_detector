
#ifndef UTIL_H
#define UTIL_H
#include "boost/filesystem/path.hpp"

std::size_t uniqueHashString(std::string str);

void addSrcFile(tv_string *, std::string);
void addSrcDir(tv_string *, std::string);
// void addSrcDirs(std::string);
void addSrcDirs(tv_string *, tv_string);
bool isInRootDirectories(std::string, tv_string);
bool isFileParsed(tv_size_t *, std::size_t);
bool isFileParsed(tv_string *, std::string);
bool isHeader(std::string);
bool fileExists(const char *);
bool dirExists(const char *);
std::string resolve(std::string);
boost::filesystem::path resolve(const boost::filesystem::path&);
void getFilesFromDirectories(tv_string, tv_string &, bool);
void updateNewNode(t_node *);
t_cursorLocation *createCursorLocation(CXSourceRange*, CXTranslationUnit *, std::string);
t_node *createNode(CXCursor*, CXSourceRange*, CXTranslationUnit *, std::string);
void printSourceRange(t_node *);
void freeParseTree(t_node *);
void hashNode(t_node *);
void hashNode(CXCursor, CXTranslationUnit *, t_node *);
void hashNodeBucketIndex(t_node *);

bool isCursorRelevant(CXCursor);
bool isNodeRelevant(t_node *, CXCursor);

#endif
