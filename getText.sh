#!/bin/bash
echo "Usage: $0 [file1.PDF] ..."
#for folder in $@; do
#    echo item: $folder
#    list=`ls $folder`
    base=$(dirname "$0")
    for fileNext in $@; do
        echo ; echo $fileNext
        file=$(basename -- $fileNext .PDF)
        #qpdf --stream-data=uncompress $file.PDF $file.pdf
        #final=$(basename -- $folder .sfdir)_$fileNext
        pdftotext -raw $fileNext > $file.txt
        sed -f $base/cleanHindi.sed $file.txt > $file.text
    done
#done

