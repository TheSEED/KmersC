extern "C" {
    #include "table.h"
};

#include "kmers.h"
#include <string.h>
#include <map>
#include <algorithm>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>

Kmers::Kmers()
{
    memset(&mtable, 0, sizeof(mtable));
    mtable.mapped_fd = -1;

    char *d = getenv("DEBUG");
    debug = d ? atoi(d) : 0;
    // fprintf(stderr, "Set debugging to %d\n", debug);
}

Kmers::~Kmers()
{
    if (mtable.mapped_fd >= 0)
    {
	unmap_table(&mtable);
    }
}

int Kmers::open_data(char *file)
{
    if (!map_table(file, &mtable))
    {
	fprintf(stderr, "error mapping %s\n", file);
	return 0;
    }

    /*
     * initialize the attr_len vector from the list in the header.
     */
    attr_len.clear();
    for (int i = 0; i < mtable.header.num_attrs; i++)
	attr_len.push_back(mtable.header.attr_len[i]);

    return 1;
}

bool max_elt(const std::pair<unsigned int, int> &lhs,
	     const std::pair<unsigned int, int> &rhs)
{
    return lhs.second < rhs.second;
}

    
int Kmers::find_hit(char *motif, std::vector<int> &attrs)
{
    int n = find_in_range(&mtable, motif, 0, mtable.len);
#if 0
    char qmotif[12];
    strncpy(qmotif, motif, mtable.header.motif_len);
    qmotif[mtable.header.motif_len] = 0;
    
    printf("find: %s => %d\n", qmotif, n);
#endif
    if (n >= 0)
    {
	char *ptr = get_motif_at(&mtable, n);
	ptr += mtable.header.motif_len;

	attrs.reserve(attr_len.size());
	attrs.clear();
	for (int i = 0; i < attr_len.size(); i++)
	{
	    int v = 0;
	    switch(attr_len[i])
	    {
	    case 1:
		v  = (int) *ptr;
		ptr++;
		break;

	    case 2:
		v = (int) ntohs(*((short *) ptr));
		ptr += 2;
		break;

	    case 4:
		v = *((int *) ptr);
		v = ntohl(v);
		ptr += 4;
		break;
	    }
	    attrs.push_back(v);
	}
	return n;
    }
    else
	return -1;
}

KmersFileCreator::KmersFileCreator(int magic, int motif_len, int pad_len, const std::vector<int> &attr_len) :
    magic(magic),
    motif_len(motif_len),
    pad_len(pad_len),
    attr_len(attr_len)
{
    if (pad_len)
	padding = (char *) calloc(pad_len, 1);
    else
	padding = 0;

    fprintf(stderr, "KmersFileCreator: magic=%x motif_len=%d pad_len=%d attr_len=< ", magic, motif_len, pad_len);
    for (std::vector<int>::const_iterator it = attr_len.begin(); it != attr_len.end(); it++)
	fprintf(stderr, "%d ", *it);
    fprintf(stderr, ">\n");
}

KmersFileCreator::~KmersFileCreator()
{
    if (padding)
	free(padding);
}

int KmersFileCreator::open_file(char *file)
{
    fp = fopen(file, "w");
    if (fp == 0)
    {
	fprintf(stderr, "error opening %s: %s", file, strerror(errno));
	return 0;
    }
    return 1;
}

int KmersFileCreator::close_file()
{
    if (fp)
    {
	fclose(fp);
	fp = 0;
	return 1;
    }
    else
	return 0;
}

int KmersFileCreator::write_file_header()
{
    int alen[32];
    int i;
    for (i = 0; i < 32; i++)
    {
	if (i < attr_len.size())
	    alen[i] = attr_len[i];
	else
	    alen[i] = 0;
    }
    ::write_file_header(fp, magic, motif_len, pad_len, alen, attr_len.size());
    return 0;
}

int KmersFileCreator::write_entry(char *motif, const std::vector<int> &values)
{
    fwrite(motif, 1, motif_len, fp);
    char cv;
    short sv;
    int iv;
    for (int i = 0; i < attr_len.size(); i++)
    {
	switch (attr_len[i])
	{
	case 1:
	    cv = (char) values[i];
	    fwrite(&cv, 1, sizeof(char), fp);
	    // printf("write char %d\n", cv);
	    break;

	case 2:
	    sv = htons((short) values[i]);
	    fwrite(&sv, 1, sizeof(short), fp);
	    // printf("write short %d\n", sv);
	    break;

	case 4:
	    iv = htonl((int) values[i]);
	    fwrite(&iv, 1, sizeof(int), fp);
	    // printf("write int %d\n", iv);
	    break;
	}
    }
    if (padding)
	fwrite(padding, 1, pad_len, fp);
    return 0;
}

int KmersFileCreator::write_entry(char *motif, int values[])
{
    fwrite(motif, 1, motif_len, fp);
    char cv;
    short sv;
    int iv;
    for (int i = 0; i < attr_len.size(); i++)
    {
	switch (attr_len[i])
	{
	case 1:
	    cv = (char) values[i];
	    fwrite(&cv, 1, sizeof(char), fp);
	    // printf("write char %d\n", cv);
	    break;

	case 2:
	    sv = htons((short) values[i]);
	    fwrite(&sv, 1, sizeof(short), fp);
	    // printf("write short %d\n", sv);
	    break;

	case 4:
	    iv = htonl((int) values[i]);
	    fwrite(&iv, 1, sizeof(int), fp);
	    // printf("write int %d\n", iv);
	    break;
	}
    }
    if (padding)
	fwrite(padding, 1, pad_len, fp);
    return 0;
}

