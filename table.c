
#include "table.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <unistd.h>

int map_table(char *file, struct motif_table *table)
{
    int fd = open(file, O_RDONLY);
    if (fd < 0)
    {
	fprintf(stderr, "Error opening %s: %s\n", file, strerror(errno));
	return 0;
    }

    memset(table, 0, sizeof(table));
    strncpy(table->mapped_file, file, sizeof(table->mapped_file));
    
    struct stat s;
    if (fstat(fd, &s) != 0)
    {
	perror("stat failed");
	exit(1);
    }
    // printf("Mapping file size %zd\n", s.st_size);
    void *ptr = mmap(0, s.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == 0)
    {
	perror("mmap failed");
	exit(1);
    }

    table->mapped_address = ptr;
    table->mapped_size = s.st_size;

    struct motif_table_header *raw_header = (struct motif_table_header *) ptr;

    /*
     * Byteswap the header.
     */
    table->header.magic = ntohl(raw_header->magic);
    table->header.motif_len = ntohl(raw_header->motif_len);
    table->header.pad_len = ntohl(raw_header->pad_len);
    table->header.num_attrs = ntohl(raw_header->num_attrs);
    table->header.data_entry_len = ntohl(raw_header->data_entry_len);
    int i;
    for (i = 0; i < 32; i++)
	table->header.attr_len[i] = ntohl(raw_header->attr_len[i]);
    
    table->table = (char *) ptr + sizeof(struct motif_table_header);

    table->len = (s.st_size - sizeof(struct motif_table_header)) / table->header.data_entry_len;
    table->mapped_fd = fd;
    
    fprintf(stderr, "mapped table, motif_len=%d pad_len=%d num_attrs=%d data_entry_len=%d len=%d\n",
	    table->header.motif_len, table->header.pad_len, table->header.num_attrs,
	    table->header.data_entry_len, table->len);
    
    return 1;
}

void unmap_table(struct motif_table *table)
{
    if (table->mapped_address)
    {
	munmap(table->mapped_address, table->mapped_size);
	close(table->mapped_fd);

	table->mapped_fd = -1;
	table->mapped_address = 0;
    }
}

int write_file_header(FILE *fp, int magic, int motif_len, int pad_len, int attr_len[32], int num_attrs)
{
    struct motif_table_header hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.magic = htonl(magic);
    hdr.motif_len = htonl(motif_len);
    hdr.pad_len = htonl(pad_len);
    int i;
    int sz = 0;
    for (i = 0; i < 32; i++)
    {
	if (i < num_attrs)
	{
	    hdr.attr_len[i] = htonl(attr_len[i]);
	    sz += attr_len[i];
	}
	else
	    hdr.attr_len[i] = htonl(0);
    }
    hdr.num_attrs = htonl(num_attrs);
    int del = motif_len + pad_len + sz;
    hdr.data_entry_len = htonl(del);
    return fwrite(&hdr, sizeof(hdr), 1, fp);
}

int find_in_range(struct motif_table *tbl, char *motif, unsigned long start, unsigned long len)
{
    /*
     * Find the midpoint, iterate.
     */

    unsigned long beg = start;
    unsigned long end = start + len;

    unsigned long mid = (end + beg) / 2;

    int mlen = tbl->header.motif_len;

    while (mid >= beg && mid < end)
    {
	//struct motif_table_entry *ent = get_table_entry(tbl, mid);
	char *tmotif = get_motif_at(tbl, mid);
	int cmp = strncmp(motif, tmotif, mlen);
	//int cmp = strncmp(motif, tbl[mid].motif, mlen);
#if 0
	{
	    char m1[30];
	    char m2[30];
	    strncpy(m1, motif, mlen);
	    strncpy(m2, tmotif,mlen);
	    m1[mlen] = m2[mlen]  = 0;
	    
	    printf("%s %s beg=%d mid=%d end=%d cmp=%d\n", m1, m2, beg, mid, end, cmp);
	}
#endif
	if (cmp < 0)
	{
	    end = mid;
	}
	else if (cmp == 0)
	{
	    return mid;
	}
	else
	{
	    beg = mid + 1;
	}
	mid = (end + beg) / 2;
	//printf("mid=%d\n", mid);
    }

    return -1;
}
