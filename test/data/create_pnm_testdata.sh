#!/bin/bash

echo ${1:?} > /dev/null

FILENAME=$1
BASENAME=${FILENAME%.*}

convert $FILENAME ${BASENAME}_bin.pbm
convert $FILENAME ${BASENAME}_bin.pgm
convert $FILENAME ${BASENAME}_bin.ppm
convert $FILENAME -compress none pbm:- | tr -s '\012\015' ' '  > ${BASENAME}_ascii.pbm
convert $FILENAME -compress none pgm:- | tr -s '\012\015' ' '  > ${BASENAME}_ascii.pgm
convert $FILENAME -compress none ppm:- | tr -s '\012\015' ' '  > ${BASENAME}_ascii.ppm
