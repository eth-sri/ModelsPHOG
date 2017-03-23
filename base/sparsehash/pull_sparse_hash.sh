#!/bin/bash
set -e
export SRC_DIR=$1
mkdir -p internal
cp $SRC_DIR/src/sparsehash/dense_hash_map dense_hash_map.h
cp $SRC_DIR/src/sparsehash/dense_hash_set dense_hash_set.h
cp $SRC_DIR/src/sparsehash/sparse_hash_map sparse_hash_map.h
cp $SRC_DIR/src/sparsehash/sparse_hash_set sparse_hash_set.h
cp $SRC_DIR/src/sparsehash/sparsetable sparsetable.h
cp $SRC_DIR/src/sparsehash/template_util.h .
cp $SRC_DIR/src/sparsehash/type_traits.h .
cp $SRC_DIR/src/sparsehash/internal/* internal

echo "DONE"

for x in $(ls *.h internal/*.h)
do
  xesc=${x/\//\\/}
  echo "sed -i -- 's/#include <sparsehash\/$xesc>/#include \"base\/sparsehash\/$xesc\"/g' *.h internal/*.h" | /bin/bash

#  echo sed -i -- "" *.h internal/*.h
done

