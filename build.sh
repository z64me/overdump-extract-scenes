mkdir -p bin

# build gfxasm.f3dex2
gcc -o bin/gfxasm.f3dex2 -DGFXASM_MAIN -DGBI_PREFIX=F3DEX2 gfxasm/src/gfxasm.c -s -Os -flto

# build gfxdis.f3dex
cp n64/src/config.h.in n64/src/config.h
gcc -o bin/gfxdis.f3dex -DF3DEX_GBI -DNDEBUG -s -Os -flto -In64/src -In64/include n64/src/gfxdis/*.c

# find-scenes
gcc -o bin/find-scenes -s -Os -flto -Wall -Wextra src/find-scenes.c

# extract-scenes
gcc -o bin/extract-scenes -s -Os -flto -Wall -Wextra -Wno-missing-field-initializers src/extract-scenes.c

# convert-room
gcc -o bin/convert-room -s -Os -flto -Wall -Wextra src/convert-room.c


