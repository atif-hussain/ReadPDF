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

../run40.sh m1.pdf
make
./pdfCleanse extract -o 42 -d 42b m1.pdf
./pdfCleanse reorder 42b > 42c
./pdfCleanse write -o 42 -d 42b~ m1.pdf
pdftotext -f 3 -l 3 -raw m1.PDF > m1.txt

Requirements: Linux built-ins, pdf2htmlEX, fontforge, & inkscape SVG viewer.
