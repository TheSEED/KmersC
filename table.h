#ifndef _table_h
#define _table_h

#ifdef __cplusplus
extern "C" {
#endif 

#include <stdio.h>

/*
 * Table of motif => score data.
 */

/*
 * This is the header that is at the beginning of the file storing
 * a motif table. Try to make it a multiple of 4 bytes in size so that
 * we can mmap the rest of the file and be properly aligned.
 *
 * pad_len is nonzero if extra padding is required to round the
 * size of each entry to a convenient alignment.
 */
    
struct motif_table_header
{
    int magic;
    int motif_len;
    int pad_len;
    int num_attrs;
    int attr_len[32];
    int data_entry_len;		/*  This should be motif_len + pad_len + sum of attr lens */
};

struct motif_table
{
    struct motif_table_header header;
    char *table;
    unsigned long len; 
    char mapped_file[1024];
    int mapped_fd;
    void *mapped_address;
    size_t mapped_size;
};

inline char *get_motif_at(struct motif_table *tbl, unsigned long n)
{
    return (tbl->table + n * tbl->header.data_entry_len);
}

int write_file_header(FILE *fp, int magic, int motif_len, int pad_len, int attr_len[32], int num_attrs);

/*
 * Find the given motif in the range.
 * Return the first item equal to or greater than the search item.
 */
int find_in_range(struct motif_table *tbl, char *motif, unsigned long start, unsigned long len);

/*
 * Compare two motifs. Return -1 if motif1<motif2, 0 if motif1 == motif2, 1 if motif1 > motif2.
 */
int compare_motifs(char *motif1, char *motif2);

int map_table(char *file, struct motif_table *table);
void unmap_table(struct motif_table *table);

#ifdef __cplusplus
}
#endif    


#endif /* _table_h */

