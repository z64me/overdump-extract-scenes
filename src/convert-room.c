/*
 * convert-room.c <z64.me>
 *
 * converts a room file's microcode from f3dex to f3dex2
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define die(X) { fprintf(stderr, X"\n"); exit(EXIT_FAILURE); }

#define STR_BIN "procDlist.bin"
#define STR_TXT "procDlist.txt"

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

/* process segment pointer (skip any that don't reference room) */
static inline void *pointer(void *room, void *bytes)
{
	unsigned v = beU32(bytes);
	unsigned char seg = v >> 24;
	unsigned ofs = v & 0xffffff;
	
	if (!v)
		return 0;
	
	if (!room)
		return 0;
	
	if (seg != 3)
		return 0;
	
	//assert(seg < 16);
	
	return ((char*)room) + ofs;
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

void procDlist(void *room, void *dlist)
{
	FILE *fp;
	unsigned char *b;
	unsigned sz;
	unsigned result;
	
	if (!dlist)
		return;
	
	/* triangle instruction parameters */
	for (b = dlist; *b != 0xb8; b += 8)
	{
		if (*b == 0xbf) /* G_TRI1 */
		{
			b[1] = b[5];
			b[2] = b[6];
			b[3] = b[7];
			b[5] = 0;
			b[6] = 0;
			b[7] = 0;
		}
	}
	b += 8;
	sz = b - (unsigned char*)dlist;
	savefile(STR_BIN, dlist, sz);
	system("bin/gfxdis.f3dex -f procDlist.bin > " STR_TXT);
	system("bin/gfxasm.f3dex2 -b < "STR_TXT" > "STR_BIN" 2> /dev/null");
	
	/* overwrite old display list with newly-converted binary */
	fp = fopen(STR_BIN, "rb");
	if (!fp)
	{
		fprintf(stderr, "file buffer fail\n");
		exit(EXIT_FAILURE);
	}
	if ((result = fread(dlist, 1, sz, fp)) > sz)
	{
		fprintf(stderr, "fread fail: %d > %d\n", result, sz);
		exit(EXIT_FAILURE);
	}
	else if (result == 0)
	{
		fprintf(stderr, "fread fail: %d != %d\n", result, sz);
		exit(EXIT_FAILURE);
	}
	fclose(fp);
	
	/* cleanup */
	remove(STR_BIN);
	remove(STR_TXT);
	
	/* walk display list, misc. patches */
	for (b = dlist; *b != 0xdf; b += 8)
	{
		/* G_DL */
		if (*b == 0xde)
		{
			/* disable animated textures */
			if (b[4] != 0x03 && (beU32(b + 4) & 0xffffff) == 0)
			{
				*b = 0;
				if (b[1])
					*b = 0xdf;
			}
			
			/* recursively process next display list */
			procDlist(room, pointer(room, b + 4));
			
			/* functions as end of dlist */
			if (b[1])
				break;
		}
		/* use tri2 instead of quad */
		else if (*b == 0x07)
			*b = 0x06;
	}
}

void procMeshHeader0(void *room, unsigned char *head, unsigned headV)
{
	unsigned char *start = pointer(room, head + 4);
	unsigned char *end   = pointer(room, head + 8);
	
	/* fixes issues in maps like death mountain crater */
	if (end < start)
	{
		end = head;
		wbeU32(head + 8, headV);
	}
	
	while (start < end)
	{
		void *dlist0 = pointer(room, start + 0);
		void *dlist1 = pointer(room, start + 4);
		
		procDlist(room, dlist0);
		procDlist(room, dlist1);
		
		start += 8; /* size of an entry */
	}
}

void procMeshHeader1(void *room, unsigned char *head, unsigned headV)
{
	unsigned char *start = pointer(room, head + 4);
	
	while (*start)
	{
		void *dlist0 = pointer(room, start);
		
		procDlist(room, dlist0);
		
		start += 4; /* size of an entry */
	}
	
	(void)headV; /* -Wunused-parameter */
}

void procMeshHeader2(void *room, unsigned char *head, unsigned headV)
{
	unsigned char *start = pointer(room, head + 4);
	unsigned char *end   = pointer(room, head + 8);
	
	/* fixes issues in maps like death mountain crater */
	if (end < start)
	{
		end = head;
		wbeU32(head + 8, headV);
	}
	
	while (start < end)
	{
		void *dlist0 = pointer(room, start +  8);
		void *dlist1 = pointer(room, start + 12);
		
		procDlist(room, dlist0);
		procDlist(room, dlist1);
		
		start += 16; /* size of an entry */
	}
}

int roomconv(void *room, unsigned roomSz)
{
	unsigned char *b;
	unsigned char *meshHeader;
	unsigned meshHeaderV;
	
	for (b = room; *b != 0x14; b += 8)
	{
		/* eliminate alternate headers */
		if (*b == 0x18)
			*b = 0x1f;
		
		/* eliminate room behavior (lost woods = too hot) */
		if (*b == 0x08)
			*b = 0x1f;
	}
	
	for (b = room; *b != 0x14; b += 8)
	{
		if (*b == 0x0A)
			break;
	}
	
	if (*b != 0x0A)
		return -1;
	
	meshHeaderV = beU32(b + 4);
	meshHeader = pointer(room, b + 4);
	
	switch (*meshHeader)
	{
		case 0x00:
			procMeshHeader0(room, meshHeader, meshHeaderV);
			break;
		
		case 0x01:
			procMeshHeader1(room, meshHeader, meshHeaderV);
			break;
		
		case 0x02:
			procMeshHeader2(room, meshHeader, meshHeaderV);
			break;
		
		default:
			fprintf(stderr, "unsupported mesh header format 0x%02x\n", *meshHeader);
			return -1;
	}
	
	(void)roomSz; /* -Wunused-parameter */
	
	return 0;
}

int main(int argc, char *argv[])
{
	char *infile;
	char *outfile;
	void *room = 0;
	size_t roomSz;
	
	fprintf(stderr, "welcome to convert-room <z64.me>\n");
	if (argc < 3)
	{
		fprintf(stderr, "not enough arguments\n");
		fprintf(stderr, "args: convert-room \"in.bin\" \"out.zmap\"\n");
		return EXIT_FAILURE;
	}
	
	infile = argv[1];
	outfile = argv[2];
	
	fprintf(stderr, "input file '%s'\n", infile);
	
	/* attempt to load room */
	if (!(room = loadfile(infile, &roomSz)))
		die("failed to load room file");
	
	/* attempt to convert map */
	if (roomconv(room, roomSz))
		die("failed to convert room file");
	
	/* write out room */
	if (!savefile(outfile, room, roomSz))
		die("failed to write room file");
	
	fprintf(stderr, "'%s' written successfully\n", outfile);
	if (room)
		free(room);
	
	return 0;
}

