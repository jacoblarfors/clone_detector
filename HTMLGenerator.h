
#include <string>
#include "llvm/Support/raw_ostream.h"

#include "config.h"
#include "tinyxml.h"

typedef std::map<std::string, t_uint16> t_srcCloneCount;

typedef struct {
    std::string fileName;
    std::string contents;
    t_uint16 lineStart;
    t_uint16 lineEnd;
    std::string columnBegin;
    std::string columnEnd;
} t_htmlCursorInfo;

class HTMLGenerator {
public :
    HTMLGenerator(std::string, t_clones);
    void generateHTMLCode();
    void createIndexHTML();
    void createSrcFileResults();

private :
    std::string outDir;
    t_clones clones;
};

