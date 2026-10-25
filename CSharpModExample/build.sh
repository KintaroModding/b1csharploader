#!/bin/sh
# Builds the example mod and copies it into the game.
# Set GAME_DIR to your BlackMythWukong folder first, for example:
#   GAME_DIR="C:/Program Files (x86)/Steam/steamapps/common/BlackMythWukong" ./build.sh
set -e

if [ -z "$GAME_DIR" ]; then
    echo "Set GAME_DIR to your BlackMythWukong folder." >&2
    exit 1
fi

dotnet build
mod_dir="$GAME_DIR/b1/Binaries/Win64/CSharpLoader/Mods/CSharpExample"
mkdir -p "$mod_dir"
cp bin/Debug/net472/CSharpExample.dll "$mod_dir/"
