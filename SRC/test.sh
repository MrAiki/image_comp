#!/bin/bash

OUTFILE="output.bin"
DECFILE="decode.sgm"

make rebuild

echo "original:compressed:compressed+xz:inputfile"
for SGMIMG in input_data/*.sgm 
do
  ./src -c $SGMIMG $OUTFILE
  ./src -d $OUTFILE $DECFILE
  diff ${DECFILE} ${SGMIMG}
  ORIGINAL_SIZE=`wc -c $SGMIMG | awk '{print $1}'`
  COMP_SIZE=`wc -c $OUTFILE | awk '{print $1}'`
  xz $OUTFILE
  XZ_SIZE=`wc -c $OUTFILE.xz | awk '{print $1}'`
  echo "$ORIGINAL_SIZE:$COMP_SIZE:$XZ_SIZE:$SGMIMG"
  rm "$OUTFILE.xz"
  rm $DECFILE
done
