# overdump-extract-scenes

This repo contains the source code for the software I wrote to help me datamine the F-Zero X overdump: specifically, this code is for locating, extracting, and fixing the surviving scene files. For those more interested in how I figured out what to do, [check out the companion article](https://z64.me/post/data-mining-the-overdump/).

**The F-Zero X overdump is not included; you must provide your own copy.**

## How to use

This is intended to be run on Linux or WSL. It is possible to build and run it on Windows as well, but not with the provided build script.

```
cd ~
git clone https://github.com/z64me/overdump-extract-scenes.git
cd overdump-extract-scenes
./build.sh
bin/extract-scenes "/path/to/your/copy/of/the/overdump"
```

If successful, a new folder named `scenes` will be made, containing every converted scene.

