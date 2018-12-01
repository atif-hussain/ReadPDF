# ReadPDF
This repository provides a utility (using other linux tools) to extract textual content from digital PDFs. 
Was developed as a community work to extract Indian Govt' public documents. 
It bypasses OCR, eliminating its error, load, & costs. Resultant file is a searchable pdf, that supports text copy. 
Supports Unicode Indic scripts like Devanagiri, etc. 

Welcome to OpenSource PDF rectifier, built as a Community service project. 
Most publicly available PDFs are digital, to reduce size. However, copying from it garbles the text in two ways. 
ReadPDF performs two functions to make a regular digital-PDF readable. 

0. standard way of storing PDF 
  PDF syntax stores all text content in /Contents stream, as a list of character-codes (Tj elements), along with 
  its /font (Tf), and position (Tm, Td). /Font maps these 8-bit character-codes to /FontDescriptor and /ToUnicode. 
  /FontDescriptor contains the glyphs to be drawn for each code in a /FontFile2 . so this is always accurate & reliable. 
  /ToUnicode contains mapping of codes/gyphs used in this file to unicode chars, which may be incorrect, due to coding compression.

.5 Preprocess
  Use the given script file (linux) to uncompress streams in the PDF and make it readable in text editor (e.g. vi filename.pdf), 
  then extract all available glyphs out of it. Say all glyphs get stored as /filename-glyphs-f4/006B.svg where f1,f2,.. are 
  font sequence in the file, 006B is the char-code. 

1. fix /ToUnicode mapping 
  ReadPDF read .svg glyph files, and reads/creates/makes a unicode mapping to it, and replaces the mapping in the PDF file. 
  It stores all glyphs in _savesFolder="../Glyphs-NewUniq/"*.svg with svg filename containing its unicode. 
  Reads preexisting mapped glyphs from _premappedFolder="../GlyphsMaster/"
  For unmapped glyphs, it visualizes the glyph, and asks user to confirm or input new unicode mapping.
  (single or multibyte unicode for complex glyphs).
  
2. fix text layout ordering. For this update section(x, y) function, and text is ordered in this order in output file. 

*Available Files*
- ../extractFonts.sh preprocessing script to extract all glyphs from pdf
- ReadPDF     source code of main application. run as one of 
    - ReadPDF extract -o 13 -d obj13.dump my.pdf   - extracts unfiltered obj#13 from my.pdf to obj13.dump
	  - ReadPDF write -o 13 -d obj13.dump my.pdf   - write/replace obj#13: obj13.dump in my.pdf as my.pdf~
	  - ReadPDF reorder obj13.dump   - edits /Content stream: Combine ET..BT blocks, & Rearranges text
	  - ReadPDF fix (0-3) *.pdf, ... - Fix unicode mappings (if 1,3), & reorder text layout (if 2,3) & write to *.PDF

- CMakeLists.txt     cmake file to build the application; build using mkdir build; cd build; cmake ..; make; 

*Instructions to Use*
  - cmake ..; make;         # in ./build/ folder requires sudo apt install podofo
  - ../extractFonts.sh *.pdf;    #generate font files *.woff and extract glyphs *.svg, also create flatedecode of pdf 
  - ReadPDF fix 3 *.pdf    # Remaps unicode's in /ToUnicode, & Reorders text in /Contents streams (my sampleOutputs used 1 not 3)
  - ../getText.sh *.PDF    # runs pdftotext to get unicode text in raw layout, and applies cleanHindi postprocessing
  
  to use partially
  - ./ReadPDF extract -o 42 -d 42.dump m1.pdf;     # extract particular object stream to a dump file
  - ./ReadPDF reorder 42.dump > 42c;               # reorder text within /Contents object stream, as per section(x,y) which defines ordering of x,y regions in Page template writes to filename~
  - ./ReadPDF write -o 42 -d 42.dump~ m1.pdf;     # write back given object stream back into PDF file, .pdf as .PDF
  - pdftotext -f 3 -l 3 -raw m1.PDF > m1.txt;     # extract text from page 3 to page 3 of PDF in content stream order

*Requirements:* 
 - Linux built-ins:
    - pdf2htmlEX, fontforge, to generate font glyph files
    - inkscape or any SVG viewer
    - podofo library to manipulate PDF
