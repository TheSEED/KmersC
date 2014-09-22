#ifndef _kmers_h
#define _kmers_h

#include "table.h"

#include <string>
#include <vector>

typedef void (*hit_callback_t)(int offset, unsigned int ff_val, unsigned int sim_val);

class KmersFileCreator
{
 public:
    KmersFileCreator(int magic, int motif_len, int pad_len, const std::vector<int> &attr_len);
    ~KmersFileCreator();

    int open_file(char *file);
    int close_file();

    int write_file_header();
    int write_entry(char *motif, const std::vector<int> &values);
    int write_entry(char *motif, int values[]);

 private:

    int magic;
    int motif_len;
    int pad_len;

    char *padding;
    FILE *fp;
    std::vector<int> attr_len;
};

class Kmers
{
 public:
    Kmers();
    ~Kmers();
    
    int debug;

    int write_file_header();
    int write_entry(char *motif, int value);
    
    int open_data(char *file);

    int find_hit(char *motif, std::vector<int> &attrs);

    int get_motif_len() { return mtable.header.motif_len; }

 private:
    int magic;
    int motif_len;
    int pad_len;
    std::vector<int> attr_len;
    int num_attrs;

    struct motif_table mtable;
};


#endif /* _kmers_h */
