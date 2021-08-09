/*
 * find-scenes.c <z64.me>
 *
 * locates scene files in a binary blob
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define die(X) { fprintf(stderr, X"\n"); exit(EXIT_FAILURE); }

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

/* read big-endian u32 */
uint32_t rbeu32(void *data)
{
	uint8_t *b = data;
	
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | (b[3]);
}

/* retrieve data pointed to by a scene segment
 * returns 0 if out of bounds or if invalid segment pointer
 */
void *scenesegment(uint8_t *datBegin, uint8_t *datEnd, uint8_t *scene, void *ptr)
{
	uint32_t addr = rbeu32(ptr);
	uint8_t *dat = scene + (addr & 0xffffff);
	
	if (!ptr)
		return 0;
	
	/* invalid segment pointer */
	if ((addr >> 24) != 0x02)
		return 0;
	
	/* out of bounds */
	if (dat < datBegin || dat >= datEnd)
		return 0;
	
	return dat;
}

/* searches for scene header pattern and prints findings */
void findheaders(uint8_t *datBegin, size_t datSz)
{
	uint8_t *dat;
	uint8_t *datEnd = datBegin + datSz;
	const uint8_t endmarker[] = { 0x14, 0, 0, 0, 0, 0, 0, 0};
	
	#define STRIDE 8 /* length of a header command */
	
	for (dat = datBegin ; dat < datEnd; dat += STRIDE)
	{
		/* potential scene header end marker match */
		if (!memcmp(dat, endmarker, sizeof(endmarker)))
		{
			uint8_t *bounds = dat - 32 * STRIDE;
			uint8_t *roomlist = 0;
			uint8_t *scene = 0;
			uint8_t *sceneEnd = 0;
			uint8_t *walk;
			uint8_t roomnum = 0;
			uint8_t i;
			unsigned datAddr;
			
			/* take care to avoid buffer underflow */
			if (bounds < datBegin)
				bounds = datBegin;
			
			/* walk backwards for a max of 32 unique header commands
			 * to confirm whether the structure matches what's typical
			 */
			for (walk = dat; walk > bounds; walk -= STRIDE)
			{
				switch (*walk)
				{
					case 0x04:
						roomlist = walk + 4;
						roomnum = walk[1];
						scene = walk; /* some scenes begin with this command */
						break;
					
					/* some scenes actually lack these commands */
					case 0x15:
					case 0x18:
						scene = walk;
						break;
				}
			}
			
			/* missing start command, so not a scene */
			if (!scene)
				continue;
			
			/* not 64-bit aligned, so not a scene */
			datAddr = (unsigned)(scene - datBegin);
			if (datAddr & 0xf)
				continue;
			
			/* no room list */
			if (roomnum == 0)
				continue;
			
			/* invalid room list pointer */
			if (!(roomlist = scenesegment(datBegin, datEnd, scene, roomlist)))
				continue;
			
			/* print file address of potential scene header */
			fprintf(stdout, "%08X\n", datAddr);
			
			/* walk room list */
			for (i = 0; i < roomnum; ++i)
			{
				uint8_t *this = roomlist + i * sizeof(uint32_t) * 2;
				uint32_t begin = rbeu32(this);
				uint32_t end = rbeu32(this + sizeof(uint32_t));
				
				/* invalid room address conditions */
				if (begin > end
					|| begin >= datSz
					|| end >= datSz
				)
				{
					fprintf(stdout, " -> ERROR\n");
					break;
				}
				
				/* list start address of each room referenced by scene */
				fprintf(stdout, " -> %08X\n", begin);
				
				/* zero each room file's contents so its header is ignored */
				memset(datBegin + begin, 0, end - begin);
				
				/* files are packed such that the end of the scene
				 * happens to be the same address as the beginning
				 * of the first room
				 */
				if (i == 0)
					sceneEnd = datBegin + begin;
			}
			
			/* it made it through the entire room list without a problem,
			 * so that data appears to be correct; now zero the scene
			 * file's contents in case it contains alternate headers
			 */
			if (i == roomnum)
				memset(scene, 0, sceneEnd - scene);
		}
	}
	
	#undef STRIDE
}

int main(int argc, char *argv[])
{
	uint8_t *dat;
	size_t datSz;
	const char *fn = argv[1];
	
	if (argc != 2 || !fn)
		die("arguments: find-scenes file.bin");
	
	if (!(dat = loadfile(fn, &datSz)))
		die("failed to load input file");
	
	findheaders(dat, datSz);
	
	/* cleanup */
	free(dat);
	return 0;
}

