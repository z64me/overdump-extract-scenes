/*
 * extract-scenes.c <z64.me>
 *
 * extracts and patches f-zero overdump scenes and rooms
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MODIFY_SCENES
#define MODIFY_ROOMS
#define CONVERT_ROOMS // f3dex to f3dex2

#define DOORSTRIDE_0x0E 0xE

struct scene {
	unsigned offset;
	const char *name;
	int doorStride;
};

/* big-endian bytes to u16 */
static inline unsigned short beU16(void *bytes)
{
	unsigned char *b = bytes;
	return (b[0] << 8) | b[1];
}

/* write u16 as big-endian bytes */
static inline void wbeU16(void *bytes, unsigned v)
{
	unsigned char *b = bytes;
	b[0] = v >>  8;
	b[1] = v;
}

/* big-endian bytes to u32 */
static inline unsigned beU32(void *bytes)
{
	unsigned char *b = bytes;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

/* write u32 as big-endian bytes */
static inline void wbeU32(void *bytes, unsigned v)
{
	unsigned char *b = bytes;
	b[0] = v >> 24;
	b[1] = v >> 16;
	b[2] = v >>  8;
	b[3] = v;
}

/* process segment pointer (skip any that don't reference scene) */
static inline void *pointer(void *scene, void *bytes)
{
	unsigned v = beU32(bytes);
	unsigned char seg = v >> 24;
	unsigned ofs = v & 0xffffff;
	
	if (!v)
		return 0;
	
	if (seg != 2)
		return 0;
	
	//assert(seg < 16);
	
	return ((char*)scene) + ofs;
}

/* minimal file loader
 * returns 0 on failure
 * returns pointer to loaded file on success
 */
void *loadfile(const char *fn, size_t *sz)
{
	FILE *fp;
	void *dat;
	
	/* rudimentary error checking returns 0 on any error */
	if (
		!fn
		|| !sz
		|| !(fp = fopen(fn, "rb"))
		|| fseek(fp, 0, SEEK_END)
		|| !(*sz = ftell(fp))
		|| fseek(fp, 0, SEEK_SET)
		|| !(dat = malloc(*sz))
		|| fread(dat, 1, *sz, fp) != *sz
		|| fclose(fp)
	)
		return 0;
	
	return dat;
}

/* minimal file writer
 * returns 0 on failure
 * returns non-zero on success
 */
int savefile(const char *fn, const void *dat, const size_t sz)
{
	FILE *fp;
	
	/* rudimentary error checking returns 0 on any error */
	if (
		!fn
		|| !sz
		|| !dat
		|| !(fp = fopen(fn, "wb"))
		|| fwrite(dat, 1, sz, fp) != sz
		|| fclose(fp)
	)
		return 0;
	
	return 1;
}

void clearActorObject(void *room)
{
#ifndef MODIFY_ROOMS
	return;
#endif
	unsigned char *b;
	
	for (b = room; *b != 0x14; b += 8)
		if (*b == 0x01 || *b == 0x0B)
			b[1] = 0;
}

const char *binstr16(unsigned short v)
{
	static char wow[17];
	int i;
	
	for (i = 0; i < 16; ++i)
		wow[i] = ((v >> (16 - (i + 1))) & 1) ? '1' : '0';
	wow[i] = '\0';
	
	return wow;
}

void ripScene(void *rom, unsigned sceneOfs, const char *name, int doorStride)
{
	char buf[1024];
	unsigned char *b = rom;
	unsigned char *scene = b + sceneOfs;
	unsigned char *w;
	unsigned char *roomList;
	unsigned char roomNum;
	unsigned char doorNum = 0;
	unsigned char *doorCmd = 0;
	unsigned char *collHeader = 0;
	unsigned char *linkList = 0;
	unsigned char linkNum = 0;
	unsigned sceneSz;
	int i;
	
	/* grab pointers to other things */
	for (w = scene; *w != 0x14; w += 8)
	{
		/* link list */
		if (*w == 0x00)
		{
			linkList = pointer(scene, w + 4);
			linkNum = w[1];
		}
		/* door list */
		else if (*w == 0x0e)
		{
			doorCmd = w;
			doorNum = w[1];
		}
		/* collision header */
		else if (*w == 0x03)
			collHeader = pointer(scene, w + 4);
		/* eliminate alternate headers */
#ifdef MODIFY_SCENES
		else if (*w == 0x18)
			*w = 0x1f;
#endif
	}
	
	/* walk header */
	for (w = scene; *w != 0x14; w += 8)
		if (*w == 0x04)
			break;
	
	/* no room list found */
	if (*w != 0x04)
		return;
	
	/* room list details */
	roomNum = w[1];
	roomList = pointer(scene, w + 4);
	
	if (!roomNum || !roomList)
		return;
	
	/* make directory */
	system("mkdir -p scene");
	sprintf(buf, "mkdir -p \"scene/%08X - %s\"", sceneOfs, name);
	system(buf);
	
	/* get scene size */
	sceneSz = beU32(roomList) - sceneOfs;
	
	/* write room_%d.zmap for each room */
	//fprintf(stdout, "%s = %d room%s\n", name, roomNum, (roomNum>1)?"s":"");
	//fprintf(stdout, "%s\n", name);
	for (i = 0; i < roomNum; ++i, roomList += 8)
	{
		char buf1[4096];
		unsigned start = beU32(roomList);
		unsigned end   = beU32(roomList + 4);
		
		/* clear actor/object lists in room file */
		clearActorObject(b + start);
		
		/* write room file to folder */
		sprintf(buf, "scene/%08X - %s/room_%d.zmap", sceneOfs, name, i);
		savefile(buf, b + start, end - start);
		
		/* convert dumped room */
#ifdef CONVERT_ROOMS
		sprintf(buf1, "bin/convert-room \"%s\" \"%s\"", buf, buf);
		system(buf1);
#endif
		
		/* zero the room */
		memset(b + start, 0, end - start);
	}
	
	/* collision header is present */
#ifdef MODIFY_SCENES
	if (collHeader)
	{
		/* disable collision cameras */
		wbeU32(collHeader + 0x20, 0);
		
		/* disable water boxes */
		wbeU32(collHeader + 0x24, 0);
		wbeU32(collHeader + 0x28, 0);
	}
	
	/* modify Link list in scene */
	if (linkList)
	{
		for (i = 0; i < linkNum; ++i, linkList += 16)
		{
			/* fixes scene 013E4860 */
			if (beU16(linkList + 14) == 0x00ff)
				wbeU16(linkList + 14, 0x0fff);
		}
	}
	
	/* modify door list in scene */
	if (doorCmd)
	{
		unsigned char *newDoor = scene + sceneSz;
		unsigned char *oldDoor = pointer(scene, doorCmd + 4);
		unsigned sceneSzOld = sceneSz;
		unsigned char *newDoorList = newDoor;
		unsigned char *oldDoorList = oldDoor;
		
		if (!doorStride)
			doorStride = 16;
		
		/* update door list pointer */
		if (doorStride != 16)
			wbeU32(doorCmd + 4, 0x02000000 | sceneSz);
		
		for (i = 0; i < doorNum; ++i, oldDoor += doorStride, newDoor += 16)
		{
			//unsigned short oldActor = beU16(oldDoor + 4);
			//unsigned short oldVar = beU16(oldDoor + 14);
			unsigned short var = 0;
			
			memmove(newDoor, oldDoor, doorStride);
			
			/* double door */
			//if (oldActor == 0x2009)
			//	var = 0x0040;
			
			/* fire temple fix */
			/* if oldActor == 0x2024 || 0x4024, use 0x0023 with var 0x0080 */
			
			/* actor number */
			wbeU16(newDoor + 4, 0x0009);
			
			/* actor variable */
			//if (doorStride == DOORSTRIDE_0x0E)
				wbeU16(newDoor + 14, var);
			
			//fprintf(stdout, "%04x %04x %s\n", oldActor, oldVar, binstr16(oldVar));
			
			sceneSz += 16;
		}
		
		/* overwrite original doors to save space */
		if (doorStride == 16)
		{
			memmove(oldDoorList, newDoorList, sceneSz - sceneSzOld);
			sceneSz = sceneSzOld;
		}
	}
#endif
	
	/* write scene.zscene to directory */
	sprintf(buf, "scene/%08X - %s/scene.zscene", sceneOfs, name);
	savefile(buf, scene, sceneSz);
	
	/* zero out the scene so i can search for others */
	memset(scene, 0, sceneSz);
}

int main(int argc, char *argv[])
{
	void *rom;
	size_t romSz;
	struct scene *item;
	struct scene list[] = {
		{ 0x010FA150, "fstdan", DOORSTRIDE_0x0E }
		, { 0x0116BF80, "dodongo's cavern" }
		, { 0x011D7CA0, "stalfos" }
		, { 0x011E6860, "stalfos-1" }
		, { 0x011F2C30, "test map with 3D ladders" }
		, { 0x01200B10, "prototype deku tree" }
		, { 0x01209B40, "gohma" }
		, { 0x01213480, "depth test" }
		, { 0x0125EE70, "prerendered shop" }
		, { 0x012875B0, "newer hyrule field" } /* has alt headers */
		, { 0x012B1C30, "kakariko" }
		, { 0x012D2DF0, "graveyard" }
		, { 0x012E8A70, "lost woods" }
		, { 0x0130C490, "kokiri forest" }
		, { 0x01335A90, "sacred forest meadow" }
		, { 0x0133CD40, "lake house" }
		, { 0x01356E50, "zora's river" }
		, { 0x0135D420, "fishing pond" }
		, { 0x01366200, "gerudo valley" }
		, { 0x0138EFC0, "hyrule castle exterior" }
		, { 0x013A8AA0, "death mountain" }
		, { 0x013C2D50, "death mountain crater" } /* old format */
		, { 0x013E4860, "cave with stalactites" } /* broken link var (change 00ff -> 0fff) */
		, { 0x013E74B0, "cave" } /* sanguinetti missed this one */
		, { 0x013F4780, "prerendered market" }
		, { 0x013F8FC0, "another prerendered market" }
		, { 0x013FC0C0, "fire temple", DOORSTRIDE_0x0E }
		, { 0x01516950, "forest temple", DOORSTRIDE_0x0E }
		, { 0x0160BB40, "horseback archery" }
		, { 0x0161B1E0, "srd test" } /* old format */
		, { 0x016225B0, "textureless scene" } /* old format */
		, { 0x01624320, "test room treasure chests" }
		, { 0x016336C0, "deku tree" } /* overwrite dodongo's cavern */
		, { 0x016AA080, "jabu jabu test" } /* old format */
		, { 0x016AD480, "chamber of sages" }
		, { 0x016BF4E0, "temple of time exterior" }
		, { 0x016C36C0, "fountain" }
		, { 0x016D0CC0, "temple of time" }
		, { 0x016E1C10, "forest temple maze" }
		, { 0x016ECFE0, "wood floor" } /* old format */
		, { 0x016EED40, "forest temple room" }
		, { 0x016F6BA0, "fire temple maze" }
		, { 0x01733F00, "kokiri prerender" }
		, { 0x0175B4C0, "another kokiri prerender" }
		, { 0x017856A0, "draw order test" }
		, { 0x01789C10, "older hyrule field" }
		, { 0x017C6920, "cave c"/*"fire temple cave"*/ }
		, { 0x017D3120, "water temple", DOORSTRIDE_0x0E } /* works best over spirit temple */
		, { 0x018C2CA0, "prerendered area" }
		, { 0x018EE2C0, "grotto" }
		, { 0x018F9B10, "gerudo training grounds" }
		, { 0x0197D860, "prerendered market entrance" }
	};
	struct scene *listEnd = list + sizeof(list) / sizeof(*list);
	
	if (argc != 2 || !argv[1])
	{
		fprintf(stderr, "arguments: extract-scenes \"your/F-Zero X Overdump.z64\"\n");
		return EXIT_FAILURE;
	}
	
	rom = loadfile(argv[1], &romSz);
	if (!rom)
	{
		fprintf(stderr, "failed to open '%s'\n", argv[1]);
		return EXIT_FAILURE;
	}
	
	for (item = list; item < listEnd; ++item)
		ripScene(rom, item->offset, item->name, item->doorStride);
	
	/* write a modified rom with scene files zero'd (debugging purposes) */
	//savefile("zero-scenes.z64", rom, romSz);
	
	free(rom);
	return EXIT_SUCCESS;
}

