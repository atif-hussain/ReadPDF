#!/bin/bash
echo "Usage: $0 [file1.pdf] ..."
echo "extracts text from pdf files using intermediate steps"
for i in $@; do
    echo item: $i
    j=$(basename -- $i .pdf)

    # Uncompress any compressed object streams to make pdf-file text-editor readable 
    #qpdf --qdf --object-streams=disable $i $j.pdf 
    qpdf --stream-data=uncompress $i $j.pdf 

    # Extract glyph sets into separate files f*.woff 
    # to make html small, bg.png's, css, js, everything is separated out & ignored 
    pdf2htmlEX --tounicode -1 --embed cfijO --auto-hint 1 --fit-width 800 --printing 0 --embed-external-font 0 --decompose-ligature 1 --space-threshold 0.30 $j.pdf # --correct-text-visibility 1 --process-type3 1 --external-hint-tool=ttfautohint 
    rm bg*.png

    wl=$(ls *.woff)
    for w in $wl; do
        ff=$(basename -- $w .woff)
        mkdir glyphs-$j-$ff
        #j="${i%.*}"
        #fontforge -lang=ff -c 'Open($1); Save($2)' f4.woff $j.sfdir
        fontforge -lang=ff -c 'Open($1); SelectAll(); Export("glyphs-'$j'-'$ff'/%U.svg")' $ff.woff   #fontforge script, donot try to read
    done

    html2text -utf8 $j.html > $j.txt

done

