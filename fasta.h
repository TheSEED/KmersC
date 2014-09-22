#ifndef _fasta_h
#define _fasta_h

/*
 * Some C code for reading & storing fasta data sets.
 */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif 

int read_fasta_item(FILE *fp, char *id, int id_len, char *data, int data_len);

#ifdef __cplusplus
}
#endif    

#endif /*  _fasta_h */
