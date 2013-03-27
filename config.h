
#include <map>
#include <vector>

#include "clang-c/Index.h"

// #define MIN_NUM_NODES 3
// #define MODULO_NODES  5
// 0.5
#define SIMILARITY_THRESHOLD 0.95

#define MIN(a,b)  (((a) < (b)) ? (a) : (b))

// #define GET_MIN_LINE_NO(line) (((line) < (0)) ? (0) : (line))
#define GET_MIN_LINE_NO(line,offset) (((line) >= (offset)) ? (line - offset) : (line))

typedef unsigned int  t_uint16;
typedef signed int    t_int16;
typedef unsigned long t_uint32;

enum NUM_NODES {
    MIN_NUM_NODES = 10,
    MODULO_NODES  = 10
};

enum FILE_LINE_OFFSET {
    LINE_OFFSET = 2
};

enum CXBinaryOpCursor {
    // HARD CODED values...
    CXCursor_IsCommutative = CXCursor_ModuleImportDecl + 1,
    CXCursor_NotCommutative,

    CXCursor_EnumEnd
};

enum CXCursor_BinaryOperatorKind {
    OPERATOR_UNKNOWN = 0,
    BO_ASSIGN,
    BO_ADD,
    BO_SUB,
    BO_MUL,
    BO_DIV,
    BO_MOD,
    BO_INCR,
    BO_DECR,
    BO_EQUAL,
    BO_NEQUAL,
    BO_GT,
    BO_LT,
    BO_GTEQ,
    BO_LTEQ,
    BO_LNOT,
    BO_LAND,
    BO_LOR,
    BO_BNOT,
    BO_BAND,
    BO_BOR,
    BO_BXOR,
    BO_BLS,
    BO_BRS
};

enum CXCursor_CompoundOperatorKind {
    // CO_
};

// struct used to store the translation units with their
// corresponding indeces
// CXTranslationunit - the translation unit
// CXIndex           - the corresponding index
typedef struct {
    CXTranslationUnit tu;
    CXIndex index;
} t_tuIndex;

// struct used to store all necessary information for
// retrieving a cursor
// CXTranslationUnit - the translation unit where the cursor
//                     resides
// CXSourceLocation  - the source location within that translation
//                     unit
typedef struct {
    t_uint16 kind; // delete
    std::string fileName;
    t_uint16 lineStart;
    t_uint16 lineEnd;
    std::string columnBegin;
    std::string columnEnd;
    /*
    CXTranslationUnit* tu;
    CXSourceLocation srcLoc;
    CXSourceRange srcRange;
    */
} t_cursorLocation;

typedef std::map<t_uint32, t_uint32> t_nodeChildTypeCount;

typedef struct t_node {
    t_cursorLocation *loc;
    CXCursorKind kind;
    t_uint32 hashValue;
    t_uint32 hashBucketIndex;
    t_uint16 numberOfChildren;
    t_nodeChildTypeCount typeMap;

    std::vector<t_node*> children;
} t_node;

typedef struct {
    CXCursorKind childKind;
    // CXSourceLocation childLoc;
    t_cursorLocation *childLoc;
} t_childCursorInfo;

// defines a vector to contain in map
// typedef std::vector<t_childCursorInfo*> t_childTypes;
typedef std::vector<t_cursorLocation*> t_childTypes;
// map to contain vector
typedef std::map<const t_uint16, t_childTypes> t_mapChildTypes;
// for counting
typedef std::map<const t_uint16, t_uint16> t_childTypeCount;

typedef struct {
    CXTranslationUnit *tu;
    t_uint16 child_count;
    t_childTypeCount child_type_count;
    t_mapChildTypes map_child_types;
} t_cursorChildTypes;

typedef struct {
    std::string fileName;
    t_cursorLocation *loc;
    t_cursorChildTypes *children;
} t_cursorInfo;

// typedef std::vector<t_cursorLocation*> t_cursorLocations;
// typedef std::vector<t_cursorInfo*> t_cursorInfoBucket;
typedef std::vector<t_node*> t_hashBucket;

// typedef std::pair<t_cursorInfo*, t_cursorInfo*> t_clonePair;
typedef std::pair<t_node*, t_node*> t_clonePair;
typedef std::vector<t_clonePair*> t_clones;
typedef std::vector<std::string> tv_string;
typedef std::vector<const char*> tvp_char;
typedef std::vector<t_tuIndex*> tvp_tuIndex;
typedef std::vector<CXFileUniqueID> tv_fileID;
typedef std::vector<std::size_t> tv_size_t;
// typedef std::map<std::string, t_cursorInfoBucket> t_clones;

// map used as hash table to store cursor locations
// and its correspoding cursor kind.
// This groups all cursors of the same type together
//typedef std::map <CXCursorKind,
//            t_cursorLocations> t_map;
// typedef std::map <t_uint16,
//             t_cursorInfoBucket> t_map;
typedef std::map <t_uint16,
            t_hashBucket> t_map;

typedef struct {
    // std::string *fileName;
    std::size_t fileName;
    bool skipFile;
} t_parentFile;

// struct used when parsing translation units to hash
// each potential clone candidate
// CXTranslationUnit - to store the current translation unit
// t_map             - to store the hash map
typedef struct {
    std::size_t tu_file;
    t_parentFile *parentFile;
    // std::string *parentFile;
    t_uint16 depth;
    CXTranslationUnit *tu;
    t_map *hash_map;
    t_node *parent;
    // parsed source files in this translation unit
    tv_size_t tu_parsed_src_files;
    // source files that have been parsed by other translation
    // unit traversals
    tv_size_t fin_parsed_src_files;
} t_hashMapParserInfo;

const char* getCursorKindName(t_uint16);


