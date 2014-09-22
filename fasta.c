
#include "fasta.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

static char linebuf[1024];

int read_fasta_item(FILE *fp, char *id, int id_len, char *data, int data_len)
{
    /* Prime next-line buffer. */
    if (linebuf[0] == 0)
    {
	if (!fgets(linebuf, sizeof(linebuf), fp))
	{
	    return 0;
	}
    }

    if (linebuf[0] != '>')
    {
	fprintf(stderr, "read_fasta_item(): invalid fasta line %s\n", linebuf);
	exit(1);
    }

    
    char *s = linebuf + 1;
    while (!isspace(*s))
	s++;
    *s = 0;
    strncpy(id, linebuf + 1, id_len);

    char *dptr = data;
    char *dend = dptr + data_len - 1;
    while (fgets(linebuf, sizeof(linebuf), fp) && linebuf[0] != '>')
    {
	char *s = linebuf;
	while (*s && dptr < dend)
	{
	    if (!isspace(*s))
		*dptr++ = *s;
	    s++;
	}
    }
    if (linebuf[0] != '>')
	linebuf[0] = 0;
    *dptr = 0;
    return 1;
}
