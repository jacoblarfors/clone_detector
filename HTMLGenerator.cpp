#include <fstream>
#include <sstream>
#include <stdlib.h>
#include "HTMLGenerator.h"

using namespace std;

const t_uint16 MAX=200;

HTMLGenerator::HTMLGenerator(string outputDir, t_clones allClones) {
    outDir = outputDir;
    clones = allClones;
}

t_htmlCursorInfo *getCursorString(string filename, t_uint16 lineStart,
                                                        t_uint16 lineEnd) {
    t_htmlCursorInfo *htmlInfo = new t_htmlCursorInfo();
    std::string str;
    std::string line;
    std::ifstream inFile(filename.c_str());
    t_uint16 lineCount = 1;
    t_uint16 offsetStart = GET_MIN_LINE_NO(lineStart, LINE_OFFSET);
    // llvm::outs() << "OFFSET START\t" << offsetStart << "\n";
    t_uint16 offsetEnd = lineEnd + LINE_OFFSET;
    while(!inFile.eof()) {
        getline(inFile, line);
        // if (lineStart <= lineCount && lineCount <= lineEnd) {
        if (offsetStart <= lineCount && lineCount <= offsetEnd) {
            if (line.compare("") == 0) {
                // llvm::outs() << "YOOOOOOO\n";
                line.append("\t");
            }
            // llvm::outs() << "\"" << line << "\"" << "\n";
            str.append(line);
            str.append("\n");
        }
        lineCount++;
    }
    inFile.close();
    htmlInfo->fileName = filename;
    htmlInfo->contents = str;
    // llvm::outs() << "lineStart = " << lineStart << "\n";
    // llvm::outs() << "lineEnd   = " << lineEnd << "\n";
    htmlInfo->lineStart = lineStart;
    htmlInfo->lineEnd = lineEnd;
    return htmlInfo;
}

// t_htmlCursorInfo *getCursorText(CXSourceRange range) { // (CXCursor cur) {
t_htmlCursorInfo *getCursorText(t_node *node) { // (CXCursor cur) {
    // intialise struct
    ostringstream oss;
    // clang_getExpansionLocation(begin, &cxFile, &lineStart, 0, NULL);
    // clang_getExpansionLocation(end, 0, &lineEnd, 0, NULL);
    // clang_getExpansionLocation(begin, &cxFile, &lineStart, &columnBegin, NULL);
    // clang_getExpansionLocation(end, 0, &lineEnd, &columnEnd, NULL);
    // cxStrFile = clang_getFileName(cxFile);
    // clang_disposeString(cxStrFile);
    t_htmlCursorInfo *cursorInfo = getCursorString(
            node->loc->fileName, 
            node->loc->lineStart,
            node->loc->lineEnd
    );
    oss << node->loc->columnBegin;
    cursorInfo->columnBegin = oss.str();
    oss.str("");
    oss << node->loc->columnEnd;
    cursorInfo->columnEnd = oss.str();
    return cursorInfo;
}

void addHighlightToCode(TiXmlElement *pre, t_htmlCursorInfo *info) {
    t_uint16 endLine = info->lineEnd;
    stringstream ss;
    // llvm::outs() << "lineStart = " << info->lineStart << "\tLINE_OFFSET = " << LINE_OFFSET << "\n";
    // llvm::outs() << "GET_MIN_LINE_NO = " << GET_MIN_LINE_NO(info->lineStart, LINE_OFFSET) << "\n\n";
    ss << /*info->lineStart;*/ GET_MIN_LINE_NO(info->lineStart, LINE_OFFSET);
    string classAttr = "brush: js; first-line: " + ss.str() + "; highlight: [";
    /*
    t_uint16 line = (GET_MIN_LINE_NO(info->lineStart, LINE_OFFSET) +
                        LINE_OFFSET);
    */
    t_uint16 line = info->lineStart;
    // llvm::outs() << "HIGHLIGHT START \t" << line << "\tHIGHLIGHT END \t " << endLine << "\n";
    for (; line <= endLine; line++) {
        // llvm::outs() << "HIGHLIGHT LINE:\t" << line << "\n";
        ss.str("");
        ss << line;
        classAttr.append(ss.str() + ",");
    }        
    classAttr.append("]");
    pre->SetAttribute("class", classAttr.c_str());
}

void HTMLGenerator::createSrcFileResults() {
    TiXmlDocument doc;
	TiXmlElement *eHTML = new TiXmlElement( "html" );
    TiXmlElement *eHEAD = new TiXmlElement( "head" );
    TiXmlElement *eTITLE = new TiXmlElement( "title" );
    TiXmlElement *eBODY = new TiXmlElement( "body" );
    
	TiXmlText * tTITLE = new TiXmlText( "YacStat Results" );
	eHTML->LinkEndChild( eHEAD );
    eHEAD->LinkEndChild( eTITLE );
    eTITLE->LinkEndChild( tTITLE );


    // create head to use code style
    TiXmlElement *meta = new TiXmlElement( "meta" );
    meta->SetAttribute("http-equiv", "Content-Type");
    meta->SetAttribute("content", "text/html; charset=UTF-8");
    TiXmlElement *script1 = new TiXmlElement( "script");
    script1->SetAttribute("type", "text/javascript");
    script1->SetAttribute("src", "scripts/shCore.js");
    script1->LinkEndChild( new TiXmlText(""));
    TiXmlElement *script2 = new TiXmlElement( "script");
    script2->SetAttribute("type", "text/javascript");
    script2->SetAttribute("src", "scripts/shBrushJScript.js");
    script2->LinkEndChild( new TiXmlText(""));
    TiXmlElement *script3 = new TiXmlElement( "script");
    script3->SetAttribute("type", "text/javascript");
    // script3->LinkEndChild( new TiXmlText("SyntaxHighlighter.defaults[\'wrap-lines\'] = true;"));
    // script3->LinkEndChild( new TiXmlText("SyntaxHighlighter.config.bloggerMode = true;"));
    script3->LinkEndChild( new TiXmlText("SyntaxHighlighter.all();"));
    script3->LinkEndChild( new TiXmlText(""));
    TiXmlElement *link = new TiXmlElement( "link" );
    link->SetAttribute("type","text/css");
    link->SetAttribute("rel", "stylesheet");
    link->SetAttribute("href","styles/shCoreDefault.css");
    TiXmlElement *style = new TiXmlElement( "style" );
    style->SetAttribute("type","text/css");
    style->LinkEndChild( new TiXmlText( "body { font-family: courier; }"));

    // style->LinkEndChild( new TiXmlText( "body .syntaxhighlighter .line { white-space: pre-wrap !important; }" ));

    eHEAD->LinkEndChild(meta);
    eHEAD->LinkEndChild(script1);
    eHEAD->LinkEndChild(script2);
    eHEAD->LinkEndChild(link);
    eHEAD->LinkEndChild(script3);
    eHEAD->LinkEndChild(style);
    
    TiXmlElement *mainTable = new TiXmlElement( "table" );
    // for each clone...
    for (t_uint16 i = 0; i < clones.size(); i++) {
        if (i > MAX) {
            break;
        }
        t_clonePair *pair = clones[i];
        // t_cursorInfo *clone_1 = pair->first;
        t_node *clone_1 = pair->first;
        // t_cursorInfo *clone_2 = pair->second;
        t_node *clone_2 = pair->second;
        t_htmlCursorInfo *info;
        TiXmlElement *tr = new TiXmlElement( "tr" );
        TiXmlElement *tr_type = new TiXmlElement( "tr" );
        TiXmlElement *h_type = new TiXmlElement( "h3" );

        // create row to contain all clone pairs found
        // to contain type of clone
        TiXmlElement *td1 = new TiXmlElement( "td" );
        // contain each clone information
        TiXmlElement *td_type = new TiXmlElement( "td" );
        TiXmlElement *h2 = new TiXmlElement( "h4" );
        TiXmlElement *pre1 = new TiXmlElement( "pre" );
        
        h_type->LinkEndChild(new TiXmlText( getCursorKindName(clone_1->kind) ));
        td_type->LinkEndChild(h_type);
        tr_type->LinkEndChild(td_type);

        // now add the code!
        /*
        info = getCursorText(clang_getCursor(*clone_1->loc->tu,
                clone_1->loc->srcLoc));
        */
        info = getCursorText(clone_1);
        string header = info->fileName;
        header.append(": [");
        header.append(info->columnBegin);
        header.append(",");
        header.append(info->columnEnd);
        header.append("]");
        // header.append(getCursorKindName(clone_1->kind));
        // add file name
        // h2->LinkEndChild( new TiXmlText( info->fileName.c_str() ));
        h2->LinkEndChild( new TiXmlText( header.c_str() ));
        // add the source code
        pre1->LinkEndChild( new TiXmlText( info->contents.c_str() ));
        addHighlightToCode(pre1, info);
        td1->LinkEndChild( h2 );
        td1->LinkEndChild( pre1 );

        // now create div with table to contain list of source files
        TiXmlElement *td2 = new TiXmlElement( "td" );
        TiXmlElement *h3 = new TiXmlElement( "h4" );
        TiXmlElement *pre2 = new TiXmlElement( "pre" );
        // now add the code!
        /*
        info = getCursorText(clang_getCursor(*clone_2->loc->tu,
                clone_2->loc->srcLoc));
        */
        info = getCursorText(clone_2);
        header = info->fileName;
        header.append(": [");
        header.append(info->columnBegin);
        header.append(",");
        header.append(info->columnEnd);
        header.append("]");
        h3->LinkEndChild( new TiXmlText( header.c_str() ));
        // add the source code
        pre2->LinkEndChild( new TiXmlText( info->contents.c_str() ));
        addHighlightToCode(pre2, info);
        td2->LinkEndChild( h3 );
        td2->LinkEndChild( pre2 );


        tr->LinkEndChild(td1);
        tr->LinkEndChild(td2);
        mainTable->LinkEndChild(tr_type);
        mainTable->LinkEndChild(tr);
    }

    eBODY->LinkEndChild(mainTable);
    eHTML->LinkEndChild( eBODY );
    // eBODY->LinkEndChild(d1);
	doc.LinkEndChild( eHTML );
	doc.SaveFile( "html/results.html" );

}

void HTMLGenerator::createIndexHTML() {
    stringstream ss;
    ss << clones.size();
    string totalClones = "Total Number of Clone Pairs Discovered: " + ss.str();
    TiXmlDocument doc;
	TiXmlElement *eHTML = new TiXmlElement( "html" );
    TiXmlElement *eHEAD = new TiXmlElement( "head" );
    TiXmlElement *eTITLE = new TiXmlElement( "title" );
    TiXmlElement *eBODY = new TiXmlElement( "body" );
    
	TiXmlText * tTITLE = new TiXmlText( "YacStat Results" );
	eHTML->LinkEndChild( eHEAD );
    eHEAD->LinkEndChild( eTITLE );
    eTITLE->LinkEndChild( tTITLE );


    eHTML->LinkEndChild( eBODY );
    
    // create div to contain YacStat title
    TiXmlElement *d1 = new TiXmlElement( "div" );
    TiXmlElement *h1 = new TiXmlElement( "h1" );
    d1->SetAttribute("align", "center");
    d1->SetAttribute("style", "width:100%;height:10%;border:6px groove blue");
    h1->LinkEndChild( new TiXmlText( "YacStat"));
    d1->LinkEndChild(h1);
    // create div to contain total number of clone pairs found
    TiXmlElement *d2 = new TiXmlElement( "div" );
    TiXmlElement *h2 = new TiXmlElement( "h2" );
    TiXmlElement *h3 = new TiXmlElement( "h3" );
    d2->SetAttribute("align", "center");
    h2->LinkEndChild( new TiXmlText( "Results Explorer"));
    h3->LinkEndChild( new TiXmlText( totalClones.c_str() ));
    d2->LinkEndChild(h2);
    d2->LinkEndChild(h3);
    // now create div with table to contain list of source files
    TiXmlElement *d3 = new TiXmlElement( "div" );
    TiXmlElement *srcTable = new TiXmlElement( "table" );
    TiXmlElement *tr1 = new TiXmlElement( "tr" );
    TiXmlElement *th1 = new TiXmlElement( "th" );
    TiXmlElement *th2 = new TiXmlElement( "th" );
    d3->SetAttribute("align", "center");
    d3->LinkEndChild(srcTable);
    srcTable->LinkEndChild(tr1);
    tr1->LinkEndChild(th1);
    tr1->LinkEndChild(th2);
    th1->LinkEndChild( new TiXmlText( "Source File" ));
    th2->LinkEndChild( new TiXmlText( "No. of Clones" ));
    
    // calculate source files and the number of clones within that source file
    t_srcCloneCount srcCloneCount;
    // for each clone...
    for (t_uint16 i = 0; i < clones.size(); i++) {
        t_clonePair *pair = clones[i];
        t_cursorLocation *loc = pair->first->loc;
        t_cursorLocation *loc2 = pair->second->loc;
        llvm::outs() << "SRC_1 = " << loc->fileName << "\n";
        llvm::outs() << "SRC_2 = " << loc2->fileName << "\n"; 
        // t_uint16 *count =
        srcCloneCount[loc->fileName]++;
        srcCloneCount[loc2->fileName]++;
        // (*count)++;
    }

    // TiXmlElement **tdEntries = (TiXmlElement**) malloc(sizeof(TiXmlElement) * srcCloneCount.size());
    TiXmlElement *trEntry;
    TiXmlElement *tdEntry1;
    TiXmlElement *tdEntry2;
    t_uint16 ind = 0;
    t_srcCloneCount::iterator it_cloneCount;
    for (it_cloneCount = srcCloneCount.begin();
                it_cloneCount != srcCloneCount.end(); ++it_cloneCount) {
        std::ostringstream sin;
        sin << (it_cloneCount->second);

        // llvm::outs() << it_cloneCount->first << "\t" << it_cloneCount->second << "\n";
        // llvm::outs() << "ind = " << ind << "\n";
        
        // tdEntries[ind] = new TiXmlElement("td");
        tdEntry1 = new TiXmlElement("td");
        tdEntry2 = new TiXmlElement("td");
        trEntry = new TiXmlElement("tr");
        // tdEntries[ind]->LinkEndChild( new TiXmlText( it_cloneCount->first.c_str() ));
        tdEntry1->LinkEndChild( new TiXmlText( it_cloneCount->first.c_str() ));
        trEntry->LinkEndChild(tdEntry1);
        // tdEntry->LinkEndChild( new TiXmlText( itoa(it_cloneCount->second) ));
        tdEntry2->LinkEndChild( new TiXmlText( sin.str().c_str() ));
        trEntry->LinkEndChild(tdEntry2);
        // srcTable->LinkEndChild(tdEntries[ind]);
        srcTable->LinkEndChild(trEntry);
        ind++;
    }

    eBODY->LinkEndChild(d1);
    eBODY->LinkEndChild(d2);
    eBODY->LinkEndChild(d3);

	doc.LinkEndChild( eHTML );
	doc.SaveFile( "html/index.html" );
}

void HTMLGenerator::generateHTMLCode() {
    // createIndexHTML();
    createSrcFileResults();
}

