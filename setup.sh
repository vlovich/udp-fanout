#!/bin/bash

cd $(dirname $0)
mkdir -p build
cd build

CMAKE_OPTS="$@"

if which xcrun > /dev/null 2>&1; then
    export CXX="$(xcrun -find clang++)"
    export CC="$(xcrun -find clang)"
    export LD="$CXX"
    PATH="$PATH:$(dirname "$CXX")"
fi

cmake $CMAKE_OPTS ../
