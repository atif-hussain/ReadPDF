:a;N;$!ba;s/\n\([\xe0\xa5\x80-\xe0\xa5\x8d]\) \?/\1/g

#see for non-greedy minimal match https://stackoverflow.com/questions/1103149/non-greedy-reluctant-regex-matching-in-sed

#move ि   to after next full consonant (not followed by silent स् )
#s/\(\xe0\xa4\xb9\)\(.*[\xe0\xa4\x84-\xe0\xa4\xb9]\)\([^\xe0\xa5\x8d]\)/\2\1\3/g 

#move र् to after previous full consonant (not followed by silent स् )
#s/\n\([\xe0\xa5\x80-\xe0\xa5\x8d]\) \?/\1/g

