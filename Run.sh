#!/bin/bash

target="Targets"
app="$1"
shift

if [[ -d "$target" ]]; then
    if [[ -d "$target/Debug/bin" ]]; then
        target="$target/Debug/bin"
    elif [[ -d "$target/Release/bin" ]]; then
        target="$target/Release/bin"
    else
        echo "Can't find folder Debug or Release inside $target"
        exit 1
    fi
else
    echo "Build the project with CMake first"
    exit 1
fi

cd "$target"
./"$app" "$@"
