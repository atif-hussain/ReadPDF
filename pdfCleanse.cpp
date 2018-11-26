#include <regex>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <codecvt>
#include <boost/algorithm/string.hpp>
#include <dirent.h>
#include <experimental/filesystem>
#include <PdfPage.h>
#include <PdfMemDocument.h>
#include <PdfObject.h>
#include <PdfDictionary.h>
#include <PdfNamesTree.h>
#include <PdfStream.h>
#include <PdfFileSpec.h>

using namespace PoDoFo;
const auto IGNORE = std::regex_constants::icase;
#define ll std::cout << __LINE__ << " " << std::flush;
#define LLL(n) {for (int i=0; i<n; i++) std::cout << "*"; std::cout << std::endl; }

//locations where glyphs for current pdf are found, orig-premapped are present, and this run saves are stored
char _localFolder[99] = "glyphs-document-name-f4/",
     _premappedFolder[] = "../GlyphsMaster/",
     _savesFolder[] = "../Glyphs-NewUniq/";

PoDoFo::PdfVecObjects objs;   //to access PdfObject by PdfReference
std::map<std::string,std::string> cmap; // map of glyphs->unicodes
std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;

std::string readGlyph(const char* file);
char* mybasename(char* filepath);
PdfObject* searchStream( PdfMemDocument& pdf, pdf_objnum objNbr);

void showObject( PdfObject* obj, const char* indent);
int FIX_UNICODE=1, REORDER_LAYOUT=2;
void traceObject( int func, PdfObject* obj, const char* indent);

void recodeCMap (const char* path_prefix, std::string& cmapping);
void recodeStreamToUnicode (PdfDictionary* obj);
void fetchPremappedGlyphs( const char* premappedFolder, std::regex ptrn, const char* savesFolder);

void reorderTextObj (PdfObject* obj);
const char* reorderText (char *orig, pdf_long& l);

void usage() {
    std::cout<< "Usage: pdfCleanse <cmd> <args..>"
             << "\n\t where <cmd> <args> are one of ..\n"
             << "\n\textract -o 13 -d obj13.dump my.pdf   - extracts unfiltered obj#13 from my.pdf to obj13.dump"
             << "\n\twrite -o 13 -d obj13.dump my.pdf   - write/replace obj#13: obj13.dump in my.pdf as my.pdf~"
             << "\n\treorder obj13.dump      - edits /Content stream: Combine ET..BT blocks, & Rearranges text"
             << "\n\tfix [0-3] [my.pdf, ...]   - Fix unicode mappings[if 1,3], & reorder text layout[if 2,3] in my.pdf & write to my.PDF"
             << "\n";
}

//#define FOO(fmt, ...) printf(fmt, ##__VA_ARGS__)
int main (int argc, char *argv[]) {
    if (argc<2) { usage(); return -1; }
    std::vector<char*> params;

    if (strcmp(argv[1],"extract")==0) {
        PdfMemDocument pdf; pdf_objnum objNbr; char *sfile; char *pdffile;
        for (int f=2; argv[f]!=nullptr; f++) {
            if (std::regex_search( argv[f], std::regex(".pdf$", IGNORE))) pdffile = argv[f];
            else if (strcmp(argv[f],"-o")==0) sscanf(argv[++f], "%d", &objNbr);
            else if (strcmp(argv[f],"-d")==0) sfile = argv[++f];
        } pdf.Load(pdffile);
        if (!pdf.IsLoaded()) { printf("Opening %s failed.\n", pdffile); return -1; }

        PdfObject* obj = searchStream(pdf, objNbr);
        PdfStream* stream = obj->GetStream(); if (!stream) return -1;
        std::ofstream myfile; myfile.open(sfile, std::ostream::out);

        char* pbuffer; pdf_long len;
        stream->GetFilteredCopy(&pbuffer, &len);  // read from pdf
        myfile << pbuffer; myfile.close();        // write to dump

    } else if (strcmp(argv[1],"write")==0) {
        PdfMemDocument pdf; pdf_objnum objNbr; char *sfile; char *pdffile;
        for (int f=2; argv[f]!=nullptr; f++) {
            if (std::regex_search( argv[f], std::regex(".pdf$", IGNORE))) pdffile=argv[f];
            else if (strcmp(argv[f],"-o")==0) sscanf(argv[++f], "%d", &objNbr);
            else if (strcmp(argv[f],"-d")==0) sfile = argv[++f];
        } pdf.Load(pdffile);
        if (!pdf.IsLoaded()) { printf("Opening %s failed.\n", pdffile); return -1; }

        PdfObject* obj = searchStream(pdf, objNbr);
        PdfStream* stream = obj->GetStream(); if (!stream) return -1;
        std::ifstream myfile; myfile.open(sfile); //std::ios::binary);

        char pbuffer[102400]; int len;
        myfile.read(pbuffer, 102400); len = strlen(pbuffer); myfile.close(); //read from dump
        //stream->Set (pbuffer, len);
        stream->Set (pbuffer, len, PdfFilterFactory::CreateFilterList(obj));
        pdf.SetWriteMode(ePdfWriteMode_Clean);
        pdf.Write(strcat(mybasename(pdffile), ".PDF"));          // write to PDF

    } else if (strcmp(argv[1],"reorder")==0) {
        for (int f=2; argv[f]!=nullptr; f++) {
            std::ifstream infile; infile.open(argv[f]); //std::ios::binary);
            char ibuffer[102400]; infile.read(ibuffer, 102400); pdf_long len = strlen(ibuffer); infile.close();

            const char* obuffer = reorderText(ibuffer, len);

            std::ofstream outfile; outfile.open(strcat(argv[f],"~"), std::ostream::out);
            outfile << obuffer; outfile.close();
        }

    } else if (strcmp(argv[1],"fix")==0) {
        std::cout << "Welcome to OpenSource PDF rectifier, built as a Community service project \n"
                     "It dis-garbles PDF text: resolve unicode Encoding, order text as per Layout. \n"
                     "Requirements: Linux built-ins, pdf2htmlEX, fontforge, & inkscape SVG viewer. \n"
                     "Ensure you are using UTF-8 encoding. \u0920 \u092D, some chars may still show incorrectly. \n"
                     "Files in _premappedFolder, must be all .svg glyphs, with hex-string{4+} in filename indicating glyph's Unicode. \n\n";

        /* Read& Build pre-existing list of unicode mapped glyphs,
         * retain only unique copy as Glyphs master list    */
        fetchPremappedGlyphs ( _premappedFolder, std::regex(".*[.]svg$"), _savesFolder);

        for (int f=3; f<argc; f++) {
            if (!std::regex_search( argv[f], std::regex(".pdf$"))) continue;
            std::cout << "Loading " << argv[f] << std::flush;

            PdfMemDocument pdf; pdf.Load(argv[f]);
            if (!pdf.IsLoaded()) { printf("Opening %s failed, skipping.\n", argv[f]); continue; }

            char* base =  mybasename(argv[f]);
            strcpy(_localFolder, base); strcat(_localFolder, "-glyphs-f0/");
            objs = pdf.GetObjects();  std::cout << " having #objects = " << objs.GetSize();
            for( int i=0; i<objs.GetSize(); i++) {
                showObject( objs[i], "\n");
                traceObject (atoi(argv[2]), objs[i], " ");
            }

            pdf.SetWriteMode(ePdfWriteMode_Clean); // Write back as .PDF
            pdf.Write(strcat(base, ".PDF")); LLL(0);
        }

    } else usage();
}



PdfObject* searchStream( PdfMemDocument& pdf, pdf_objnum objNbr) {
    PdfVecObjects& objs = pdf.GetObjects();
    const PdfReference ref(objNbr, 0);
    PdfObject *obj = objs.GetObject(ref);
    if (!obj->HasStream()) { std::cout<<"Error: Requested Object Stream not found\n"; return nullptr;}
    return obj; //->GetStream();
}

char* mybasename(char* filepath) {
    //follows POSIX version of basename, which modifies its argument
    char *wstart = strrchr(filepath, '/'); if(wstart) wstart++; else wstart=filepath;
    char *wend = strrchr(wstart, '.'); if(wend) wend[0]= '\0';
    std::cout << ". filename is " << wstart << std::endl;
    return wstart;
}

void changeUnicode (std::string& cmapping, std::string& unicode1, std::string& unicode2 ) {
    if (unicode2.size()<=4) boost::replace_all( cmapping, unicode1, unicode2);
    else  boost::replace_all( cmapping, unicode1, unicode2.insert(0, "[").append("]") );
}

std::string readGlyph(const char* file)
{
    /* Read a glyph .svg file and returns glyph containing "<svg>...</svg>" string
    */
    FILE *fp;
    fp = std::fopen(file, "r");
    if (!fp) return "NULL";

    char buffer[4096]; std::fread(buffer, 4096-1, 1, fp); std::fclose(fp);
    if (buffer[0]==0) return "NULL";
    std::string strbuffer(buffer);
    for (size_t pos=0; (pos = strbuffer.find ("\n",pos) ) != std::string::npos; )   strbuffer[pos]='\t';

    std::smatch reslt;
    bool b = std::regex_search(strbuffer, reslt, std::regex("<svg.*svg>"));
    //std::cout << reslt.str()  << std::endl;
    return std::string(reslt.str());
}



void fetchPremappedGlyphs( const char* premappedFolder, std::regex ptrn, const char* savesFolder)
{
    /* Read& Build pre-existing list of unicode mapped glyphs,
     * retain only unique copy as Glyphs master list
     * builds: map <string, string> cmap
    */
    struct dirent *ent;
    DIR* dir = opendir(premappedFolder);
    if (dir == NULL) { std::cout << premappedFolder << " directory not found\n"; return; }
    std::cout << "Using premappedFolder " << premappedFolder <<std::endl;
    int count = 0;
    while (ent=readdir(dir))
    {
        if (!std::regex_match(std::string(ent->d_name), ptrn)) continue;
        count++;

        std::string glyph = readGlyph( std::string(premappedFolder).append(ent->d_name).c_str());
        if (cmap.find(glyph) != cmap.end()) continue;
        //std::cout << "New glyph found: " << ent->d_name << " in " << ent->d_name << std::endl;

        /* Add to cmap: [glyph-string -> unicodepoint]    */
        std::cmatch reslt;
        std::regex_search( ent->d_name, reslt, std::regex("[0-9A-Fa-f]{4}+"));
        cmap[glyph] = reslt[0];

        std::string cmd("cp "); cmd += std::string(premappedFolder) += std::string(ent->d_name)
                     += std::string(" ") += std::string(savesFolder) += std::string(ent->d_name);
        boost::replace_all( cmd, "(", "\\("); boost::replace_all( cmd, ")", "\\)");
        system(cmd.c_str());
    }
    std::cout << "read " << count << " svg files; #unique glyphs are now " << cmap.size() << std::endl;
}

#define printf(fmt, ...)
void showObject (PdfObject* obj, const char* indent)
{
    /* show current Object-Ref (& its size) */
    printf("%s", indent);

    PdfReference ref = obj->Reference();
    printf("OBJ[%s]", ref.ToString().c_str());

    unsigned int len = obj->GetObjectLength(ePdfWriteMode_Clean);
    printf("(%d): ", len);
}

void traceObject (int func, PdfObject* obj, const char* indent)
{
    /* show current Object-details (type-size) & it's children
     *   Dictionary-len{ /key=/value ...}
     *   Array-len{ elements, ... }
     *   Name-myname
     *   Number-2345
     *   Reference[reference-string]
    */
    printf("%s", indent);
    PdfName name = obj->GetDataTypeString();
    printf("%s", name.GetName().c_str());

    EPdfDataType type = obj->GetDataType();
    switch (type)
    {
    case ePdfDataType_Null:
        printf(" %s", "null");
        break;

    case ePdfDataType_Bool:
        printf(" %s", obj->GetBool()?"true":"false");
        break;

    case ePdfDataType_Number:
        printf(" %ld", obj->GetNumber());
        break;

    case ePdfDataType_Real:
        printf(" %f", obj->GetReal());
        break;

    case ePdfDataType_String:    //literal string (..)
    case ePdfDataType_HexString: // hexadecimal <....>
        printf(" %s\t", obj->GetString().GetString());
        break;

    case ePdfDataType_Name:
        printf(" %s", obj->GetName().GetName().c_str());
        break;

    case ePdfDataType_Array:
        printf("-%ld{", obj->GetArray().GetSize());
        for (int j=0; j<obj->GetArray().GetSize(); j++)
            traceObject (func, &(obj->GetArray()[j]), ", ");
        printf("%s", "}");
        break;

    case ePdfDataType_Dictionary:
        {PdfDictionary dict = obj->GetDictionary();
         printf("-%ld{", dict.GetSize());

        for (TCIKeyMap it = dict.begin(); it != dict.end(); it++) {
            printf(" /%s", it->first.GetName().c_str());
            traceObject (func, it->second, "=");
            if (func & FIX_UNICODE) if (it->first == PdfName("ToUnicode")) recodeStreamToUnicode(&dict);
            if (func & REORDER_LAYOUT) if (it->first == PdfName("Contents")) reorderTextObj(it->second);
        } }
        break;

    case ePdfDataType_Reference:
        printf("[%s]", obj->GetReference().ToString().c_str());
        break;

    default:
        printf(" %s", obj->GetDataTypeString());
    }
    if (obj->HasStream()) {
        PoDoFo::PdfStream* data = obj->GetStream();
        printf("\t STREAM of size %ld ", data->GetLength());
    }
}

void recodeStreamToUnicode( PdfDictionary* fontdict) { //for this /Font
    /* Return: Rectify unicode's in /ToUnicode map of fontdict
     * input: /ToUnicode reference object
     * action: for glyphs in localFolder, lookup their unicode in premappedFolder
     * @see: recodeCMap(...) for details further
    */

    //update subFolder to search font files (*.svg)
    _localFolder[strlen(_localFolder)-2]++;
    printf(" subfolder=%s ", _localFolder);

    //note down /BaseFont
    PdfObject * nameobj = fontdict->GetKey(PdfName("BaseFont"));
    if (nameobj->IsReference())  nameobj = objs.GetObject( nameobj->GetReference());
    std::string font = (nameobj->IsName())? nameobj->GetName().GetName() : "Unknown Font";
    if (!std::regex_search(font, std::regex("Mangal"))) return;
    std::cout << " \n\n/BaseFont=" << font << " being updated.";

    // Fetch /ToUnicode stream
    PdfObject * obj = fontdict->GetKey(PdfName("ToUnicode"));
    if (obj->IsReference())  obj = objs.GetObject( obj->GetReference());
    if (!obj->HasStream()) return;

    PdfStream* uni_map_str = obj->GetStream();
    char *orig; pdf_long l; uni_map_str->GetFilteredCopy( &orig, &l);
    printf("\n***** /ToUnicode stream of length %ld *****\n", l);

    /* Recode /ToUnicode Mapping stream */
    std::vector<std::string> lines; std::string catlines;
    for (char *from=orig, *to; *from!='\0'; from=to+1)
    {
        to=strchr(from,'\n');
        std::string line(from, to); printf ("\n%s", line.c_str());
        boost::replace_all( line, "[<", "<");
        boost::replace_all( line, ">]", ">");

        // Recode
        recodeCMap( _localFolder, line);

        lines.push_back(line);
        catlines.append(line).append("\n");
    }
    boost::replace_all( catlines, "<[", "[<");
    boost::replace_all( catlines, "]>", ">]");

    uni_map_str->Set( catlines.c_str(), catlines.size());
}
#undef printf

void recodeCMap (const char* path_prefix, std::string& cmapping)
{
    /* Return: Modify unicode in the mapping string in /ToUnicode map
     * input: cmapping is a /ToUnicode mapping string e.g. <2e> <2e> <90F1>
     *        the glyph file for char is available as path_prefix/002E.svg
     * action:
     *   if the glyph for 2e has valid pre-mapped unicode, use it
     *   else open the glyph file and ask for new input unicode hexstring
    */
    std::smatch codes;
    std::regex chmap( "<([0-9A-Fa-f]+)> <([0-9A-Fa-f]+)> <([0-9A-Fa-f]+)>");

    if (!std::regex_search(cmapping, codes, chmap)) return;
    // codes[] = {full-line, chcode, chcode, unicode1}

    if (codes[2].str().compare(codes[1].str()) !=0) return;
    std::string chcode = codes[1].str();
    std::transform( chcode.begin(), chcode.end(), chcode.begin(), ::toupper);
    std::string unicode1 = codes[3].str();

    char32_t codept=0;
    for (std::string::iterator j=unicode1.begin(); j<unicode1.end(); j++) 
        if (*j>='a') codept = (codept<<4)+ (*j)-'a'+10;
        else if (*j>='A') codept = (codept<<4)+ (*j)-'A'+10;
        else codept = (codept<<4)+ (*j)-'0';

    std::string glyphfile = std::string(path_prefix); glyphfile += (std::string("00")) += chcode += std::string(".svg");
    std::string glyph = readGlyph (glyphfile.c_str());


    /* skipped if not Mangal font ie. use existing mapping in pdf
     * if cmap mapping found in _premappedFolder, use this corrected mapping
     * else display glyph & show old-unicode existing in pdf (both hex value & devagiri char)
     *      press enter to retain pre-existing map
     *      type new unicode to change
     * Added: support to input multiple-unicode-chars for a glyph र्र्र्र्र्र्र्र्र्र्;
     */
    if (cmap.find(glyph) != cmap.end()) {
        // if glyph already exists in cmap
        std::string unicode2 = cmap.find(glyph)->second;
        if (unicode2.size()&3) std::cout << "\nError: non-standard length Unicode string found!\n";

        changeUnicode (cmapping, unicode1, unicode2 );

    } else {
        //std::cout << "New glyph found: " << glyphfile << " in " << path_prefix << std::endl;
        std::string unicode2;
        do {
            std::ifstream f(glyphfile.c_str()); if (!f.good()) return;
            std::cout << "/ToUnicode CMap:"<<chcode<<"->"<<unicode1<<"(" <<convert.to_bytes(codept)<< "). Accept or Enter new: " << std::flush;
            std::string open_glyph("inkscape "); open_glyph += glyphfile;
            system(open_glyph.c_str()); //("inkscape testdoc-f4-glyphs/002A.svg");

            getline( std::cin, unicode2);
            //std::cout << "\ninput " << unicode2.c_str() << " is " << unicode2.length() << " long.\n";


            /* special char sequences to handle oft-repeated silent multi-char mixed Indic glyphs:
            *  Silent chars: a ा  e ि  i ी  u ु   U ू   o ो    ,'्   r र्   R ्र    */
            if (unicode2.size()==0)       unicode2 = unicode1; //retain existing cmapping
            if (unicode2.size()==1) {
                if (unicode2.compare("a")==0) unicode2 = std::string("093e");
                if (unicode2.compare("e")==0) unicode2 = std::string("093f");
                if (unicode2.compare("i")==0) unicode2 = std::string("0940");
                if (unicode2.compare("u")==0) unicode2 = std::string("0941");
                if (unicode2.compare("U")==0) unicode2 = std::string("0942");
                if (unicode2.compare("o")==0) unicode2 = std::string("094b");
                if (unicode2.compare(",")==0) unicode2 = unicode1 + std::string("094d");
            }
            boost::replace_all( unicode2, "'", "094d");
            boost::replace_all( unicode2, ",", "094d");
            boost::replace_all( unicode2, "r", "0930094d");
            boost::replace_all( unicode2, "R", "094d0930");


            if (std::regex_match(unicode2, std::regex("^[0-9A-Fa-f]{4}+$"))) {
                /* use the new unicode2 -
                 * 1. add it to cmap table
                 * 2. save glyph to _savesFolder with fullname
                 * 3. replace in pdf cmapping
                 */
                cmap[glyph] = unicode2;

                std::string newfile(_localFolder); newfile.insert(0, "/");
                if (newfile.find_last_of("/") == newfile.size()-1)  newfile.resize(newfile.size()-1);
                std::cout << "(" << newfile << ")";
                newfile = newfile.substr(newfile.find_last_of("/\\") + 1);
                std::string copycmd("cp "); copycmd .append(glyphfile) .append(" ") .append(_savesFolder)
                        .append(newfile) .append("_") .append(chcode) .append("_") .append(unicode2) .append(".svg");
                std::cout << copycmd << std::endl;
                system(copycmd.c_str());

                changeUnicode (cmapping, unicode1, unicode2);
                break;
            }
        } while (true);
    }
}

int section (int x, int y) {
    /* This Programme orders text in PDF pages by increasing section number.
     * Customize this function based on application need.
     * @input: x y (in 72dpi) from bottom-left, for A4 within [0 0 595 842]
     * @return: return section# based on position of y,x in page
    */
    y = -y +842; // graphics cm offset
    if (y>15000) return -1;    //footnote
    else if (y<=0) return -2;  //title
    int row = std::ceil (y/1500);
    int column = (x<3300)?1: (x<6600)?2 :3;
    return 3*row+column;
}

// initialize Text state(s); may be set outside txtmode; values are retained across stream;
struct State {
    //text-States
    std::string Tf=""; //Tf font,size - apply last set state
    std::string Tr="0 Tr";//renderMode - apply last set state
    std::string Tc="0 Tc"; //charSpace - apply last set state
    std::string Tw="0 Tw"; //wordSpace - apply last set state
    std::string Tz="100 Tz"; //Th=scale - apply last set state
    std::string Tl="0 TL";   //leading - apply last set state; set leading (linespacing) is used by T*, ', "
    std::string Ts="0 Ts";      //rise - apply last set state
    //std::string Tk= "true TK gs"; //knockout - applies to entire <BT..ET> textObj

    //position-States
    // Text Rendering based on 2 states: Tlm = line-origin, Tm = glyph-origin; Tm=Tlm at line-start, then +=glyph-width
    //std::string Tm="0 Ts"; //text matrix - apply last set state
    //std::string Tl="0 Ts"; //text line matrix - apply last set state
    std::string Tm="1 0 0 1"; //0 0 Tm  text rendering matrix combines Tm & current trans' matrix
                 // "a b c d e f" for Tm={[a b 0],[c d 0],[e f 1]} - apply last set state
    std::string T0="T*"; // move to start of next line equivalent to: 0 <Tl> Td
    struct {int x=0, y=0;} Td;  // concatenate Tm = Tlm = x,y offset from prev Tlm
    int s=-1;   // tx ty TD; is equi-to  -ty TL; tx ty Td; i.e Td & also set Tl= -ty


    //rendering-States
    std::string Tj;
    // tj Tj; //show a text string tj
    // tj '     //nextline & show text; equi to T*; tj Tj;
    // tw tc tj " //nextline, Tw, Tc & show text; equi to tw Tw; tc Tc; T*; tj 'Tj;
    // array TJ; //show multiple text strings, with individual glyph positioning
    //std::string Tm="0 Ts"; //text matrix - apply last set state

};

//#define printf(fmt, ...)
void writeback( std::vector<State>& groups, State current, std::string& catlines)
{
    std::stable_sort( groups.begin(), groups.end(), [](State g1, State g2) {
        return (g1.s!=g2.s)?(g1.s<g2.s) : (abs(g1.Td.y-g2.Td.y)>100)?(g1.Td.y>g2.Td.y) : (g1.Td.x<g2.Td.x); } );

printf("writing %ld groups\n", groups.size());
    // generate states changed from previous, before this Tj element
    /*current = initial;*/ current.Tm = ""; //reset current.Tm to create a Tm entry at start
    for (const State& state: groups)
    {
        if (state.Tm.compare(current.Tm)!=0) {
            printf ("wrot Tm=%s;Td=%d,%d;", state.Tm.c_str(),state.Td.x, state.Td.y);
            catlines.append(state.Tm)   //.append("\n");
                    .append(" ").append(std::to_string(state.Td.x))
                    .append(" ").append(std::to_string(state.Td.y))
                    .append(" Tm\n"); current.Td=state.Td; current.Tm=state.Tm;
        }
        else if ((state.Td.x != current.Td.x) || (state.Td.y != current.Td.y)) {
            catlines
                    .append(std::to_string(state.Td.x-current.Td.x)).append(" ")
                    .append(std::to_string(state.Td.y-current.Td.y)).append(" Td\n");
            printf( "gott Td=%d,%d;", state.Td.x, state.Td.y);
            printf( "wrot Td=%d,%d;", (state.Td.x-current.Td.x), (state.Td.y-current.Td.y));
            current.Td=state.Td; }

        if (state.Tf.compare(current.Tf)!=0) {catlines.append(state.Tf).append("\n"); current.Tf=state.Tf; }
        if (state.Tr.compare(current.Tr)!=0) {catlines.append(state.Tr).append("\n"); current.Tr=state.Tr; }
        if (state.Tc.compare(current.Tc)!=0) {catlines.append(state.Tc).append("\n"); current.Tc=state.Tc; }
        if (state.Tw.compare(current.Tw)!=0) {catlines.append(state.Tw).append("\n"); current.Tw=state.Tw; }
        if (state.Tz.compare(current.Tz)!=0) {catlines.append(state.Tz).append("\n"); current.Tz=state.Tz; }
        if (state.Tl.compare(current.Tl)!=0) {catlines.append(state.Tl).append("\n"); current.Tl=state.Tl; }
        if (state.Ts.compare(current.Ts)!=0) {catlines.append(state.Ts).append("\n"); current.Ts=state.Ts; }

        catlines.append(state.Tj).append("\n");
        printf("\tTf=%s ", state.Tf.c_str());
        printf("\tTj=%s;\n", state.Tj.c_str());
    }
}

const char* reorderText (char *orig, pdf_long& l) {
    // ToUnicode map stream is ready, modify as needed inplace without changing its length.
    State blockStart; //stores State at the beginning of ET..BT Text block
    State current; //stores current State as each line is read
    std::stack<State> states; //stack graphics states from outside Text blocks, q for save, Q for restore

    std::string catlines; // store entire modified /stream
    std::vector<State> groups; //store Tj-groups (state+Tj), within ET..BT section of /stream
    std::smatch ptn;
    bool txtmode = false;
    for (char *from=orig, *to; from<orig+l; from=to+1)
    {
        to=strchr(from,'\n');
        std::string line(from, to);
        //printf ("\n%s", line.c_str());

        /*  switch () {
                !txtmode  retain line
                !txtmode && BT  retain line  txtmode=true
                txtmode   linegroups.push(line)
                txtmode && ET  insert(sort(linegroups))   txtmode=false
        } */

        if (!txtmode) {
            // non-txtmode written as is; blockStart state initialized on BT, used on ET
            if (std::regex_search( line, std::regex("^\\s*BT\\s*$"))) {
                printf("\nBT\n");
                blockStart=current; txtmode = true;

            } else {
                //leave as-is any non-Text lines outside ET..BT
                catlines.append(line).append("\n");

                //update State where affects Text lines
                if (std::regex_search( line, std::regex("^\\s*q\\s*$"))) {
                    states.push(current);
                    printf("\nsaving Tm: %s %d %d\n", current.Tm.c_str(), current.Td.x, current.Td.y);
                } else if (std::regex_search( line, std::regex("^\\s*Q\\s*$"))) {
                    printf("\noverriding Tm: %s %d %d\n", current.Tm.c_str(), current.Td.x, current.Td.y);
                    current = states.top();
                    printf("\nrestored Tm: %s %d %d\n", current.Tm.c_str(), current.Td.x, current.Td.y);
                    states.pop();
                }

                /*else if (std::regex_search( line, ptn, std::regex("(.*)\\s+(" "((-?[0-9]+)\\s+){2}" ")cm\\b"))) {
                    //std:: cout << "cm match ("<<line<<"): "; for (auto& e: ptn) std:: cout << e.str() << ";"; std:: cout << "\n";
                    //current.Tm = ptn[0].str();
                    current.Tm = ptn[1].str();
                    int dx, dy; sscanf( ptn[2].str().c_str(), "%d %d ", &dx, &dy);
                    current.Td.x = dx; current.Td.y = dy;
                    current.s = section(current.Td.x, current.Td.y);
                    printf("read Tm=%s;Td=%d,%d;", current.Tm.c_str(), dx, dy);

                }*/
            }
            continue;
        }

        if (std::regex_search( line, std::regex("^\\s*ET\\s*$"))) {
            txtmode = false;
            //continue; //skip to combine [BT..ET] sections
            catlines.append("BT\n");
            writeback( groups, blockStart, catlines); groups.clear();
            catlines.append("ET\n"); printf("ET\n");
            continue;
        }
//#define printf(fmt, ...)
        // Now, these are actions within a txtMode

        // set_state(), just record this state & apply to all subsequent lines ..into State current
        if (std::regex_search( line, ptn, std::regex("\\s(T[cwzlfrs])\\b"))) {
            printf( "\t%s=%s ", ptn[1].str().c_str(), line.c_str());
            char Tstate = ptn[1].str().at(1);
            switch (Tstate) {
            case 'f': current.Tf = line; break;
            case 'r': current.Tr = line; break;

            case 'c': current.Tc = line; break;
            case 'w': current.Tw = line; break;
            case 'z': current.Tz = line; break;
            case 'l': current.Tl = line; break;
            case 's': current.Ts = line; break;
            default: printf("Could not recognize:%s", line.c_str());
            }
            continue;
        }

        // from relative positions, record the absolute position ..into State current
        if (std::regex_search( line, ptn, std::regex("(.*)\\s+(" "((-?[0-9]+)\\s+){2}" ")Tm\\b"))) {
            //std:: cout << "Tm match ("<<line<<"): "; for (auto& e: ptn) std:: cout << e.str() << ";"; std:: cout << "\n";
            //current.Tm = ptn[0].str();
            current.Tm = ptn[1].str();
            int dx, dy; sscanf( ptn[2].str().c_str(), "%d %d ", &dx, &dy);
            current.Td.x = dx; current.Td.y = dy;
            current.s = section(current.Td.x, current.Td.y);
            printf("read Tm=%s;Td=%d,%d;", current.Tm.c_str(), dx, dy);
            continue;
        }
        if (std::regex_search( line, ptn, std::regex("\\sTd\\b"))) {
            int dx, dy; sscanf( line.c_str(), "%d %d ", &dx, &dy);
            printf( "read Td=%d,%d;", dx, dy);
            //std:: cout << "Td ("<<line<<") read: " << ptn.prefix().str() << ";" << ptn[0].str() << ";" << dx << ";" << dy;
            current.Td.x += dx; current.Td.y += dy;
            current.s = section(current.Td.x, current.Td.y);
            printf( "made Td=%d,%d;", current.Td.x, current.Td.y);
            continue;
        }

        // render cmd, read this line-text, & store with current state ..into vector<State> groups
        if (std::regex_search( line, std::regex("\\sTj\\b", IGNORE ))) {
            printf( "\tTj=%s;\n", line.c_str());
            State g = current; g.Tj = line;
            groups.push_back(g);
            continue;
        }

        printf("Could not recognize:%s", line.c_str());
    }

    // Now add a combined ET..BT block, and reorder Text Layout ..write into vector<string> catlines
    if (groups.size()>0) {
        catlines.append("BT\n");
        State newState;
        writeback( groups, newState, catlines); groups.clear();
        catlines.append("ET\n"); printf("..ET\n");
    }
    //std::cout << catlines << std::endl;
    l = catlines.size();
    return catlines.c_str();
}

void reorderTextObj (PdfObject* obj)
{
    /* action: Combine ET..BT blocks, and Rearrange text within /Contents stream
     * lines are {operands,..} operator; that change Text Rendering State
     * operators maybe text-State/text-Positioning/text-Showing
    */
    std::cout << " \n/Contents being updated.";
    if (obj->IsArray()) obj = &(obj->GetArray()[0]);
    if (obj->IsReference())  obj = objs.GetObject(obj->GetReference());
    if (!obj->HasStream()) return;

    PdfStream* contents_str = obj->GetStream();
    char *orig; pdf_long l; contents_str->GetFilteredCopy( &orig, &l);
    printf("\n***** /Contents text stream of length %ld *****\n", contents_str->GetLength());

    const char* catlines = reorderText(orig, l);
    contents_str->Set( catlines, l);
}

#undef printf
