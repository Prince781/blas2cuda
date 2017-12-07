#!/bin/sh
# blas2cuda.sh
# preload libblas2cuda.so and run with a tracker file

trackfile=$1
progname=$2
BLAS2CUDA=../libblas2cuda.so

if [ -z $trackfile ] || [ -z $progname ]; then
    echo "Usage: $0 <objtrackfile> <command> ARGS..."
    exit 1
fi

if [ ! -e $BLAS2CUDA ]; then
    if ! make -C $(dirname $BLAS2CUDA); then
        exit 1
    fi
fi

env LD_PRELOAD=$BLAS2CUDA BLAS2CUDA_OPTIONS="track=$trackfile" $progname ${@:3}