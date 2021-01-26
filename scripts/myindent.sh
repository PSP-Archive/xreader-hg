#!/bin/sh

if [ $# -lt 1 ]; then
	echo "Auto Indent Script "
	echo "Usage: $0 <source_and_header_files>"
fi

while [ $# -gt 0 ];
do
	SRC="$1"
	INDENTFLAG="-kr -ut -bap -br -bls -brs -ts4 -l160 -sob -cdw -cli4 -bls -i4 -nce"
	echo "Indenting $SRC"
	test ! -f "$SRC" && (echo "$SRC doesn't exists"; shift; continue)
#	dos2unix -c ISO "$SRC"
	cat "$SRC" | tr -d "\r" > tmp.txt
	mv tmp.txt "$SRC"
	INDENTFLAG="-kr -ut -bap -br -bls -brs -ts4 -l160 -sob -cdw -cli4 -bls -i4 -nce"
	indent $INDENTFLAG "$SRC"
	INDENTFLAG="-kr -ut -bap -br -bls -brs -ts4 -l160 -sob -cdw -cli4 -bls -i4 -ce"
	indent $INDENTFLAG "$SRC"
#	dos2unix -c ISO "$SRC"
	cat "$SRC" | tr -d "\r" > tmp.txt
	mv tmp.txt "$SRC"
	shift
done

