#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#ifdef __cplusplus
}
#endif

#include "kmers.h"

MODULE = KmersC		PACKAGE = KmersC		

Kmers *
Kmers::new()
	CODE:

	RETVAL = new Kmers();
	
	OUTPUT: 
	RETVAL

void
Kmers::DESTROY()

int
Kmers::get_motif_len()


int
Kmers::open_data(char  * file)

int
Kmers::find_all_hits(char *seq, int length(seq), AV *list)
	CODE:
	{
	    char *s = seq;
	    
	    size_t slen = XSauto_length_of_seq;
	    if (slen < THIS->get_motif_len())
	        return;
	    
	    //printf(stderr, "seq len=%d\n", slen); 

	    for (int i = 0; i <= slen - THIS->get_motif_len(); i++)
	    {
		char *s = seq + i;
		// fprintf(stderr, "find : i=%d\n", i);
		std::vector<int> attrs;
		int n = THIS->find_hit(s, attrs);
		if (n >= 0)
		{
#if 0
		    char motif[12];
		    strncpy(motif, hit->motif, MOTIF_LEN);
		    motif[MOTIF_LEN] = 0;
		    char smotif[12];
		    strncpy(smotif, s, MOTIF_LEN);
		    smotif[MOTIF_LEN] = 0;
		    
		    printf("Hit q=%s t=%s: ff=%d sim=%d i=%d off=%d\n", smotif, motif, hit->ff_value, hit->sim_value, i, offset);
#endif
		    //printf("Have hit i=%d n=%d asize=%d\n", i, n, attrs.size());
		    /*
		     * Turn the attrs into a list and push to the result list.
		     */
		    SV *vals;
		    AV *av = newAV();
		    av_push(av, newSViv(i));
		    av_push(av, newSVpvn(s, THIS->get_motif_len()));
		    for (int ai = 0; ai < attrs.size(); ai++)
		    {
			av_push(av, newSViv(attrs[ai]));
		    }
		    av_push(list, (SV *) newRV((SV *) av));
		    SvREFCNT_dec(av);
		}
		s++;
	    }
	}
OUTPUT:
	RETVAL

MODULE = KmersC PACKAGE = KmersFileCreator

KmersFileCreator *
KmersFileCreator::new(int magic, int motif_len, int pad_len, AV * attr_len_list)
	CODE:
	{
	    std::vector<int> alen;

	    for (int i = 0; i <= av_len (attr_len_list); i++) {
		SV **elem = av_fetch (attr_len_list, i, 0);

		if (!elem || !*elem) {
		    croak ("foo");
		}

		alen.push_back(SvIV(*elem));
	    }

	    RETVAL = new KmersFileCreator(magic, motif_len, pad_len, alen);
	}    
	
	OUTPUT:
	RETVAL

int
KmersFileCreator::write_file_header()

int
KmersFileCreator::open_file(char *file)

int
KmersFileCreator::close_file()

int
KmersFileCreator::write_entry(char *motif, AV *values)
	CODE:
	{
	    std::vector<int> avals;

	    for (int i = 0; i <= av_len (values); i++) {
		SV **elem = av_fetch (values, i, 0);

		if (!elem || !*elem) {
		    croak ("foo");
		}

		int val = SvIV(*elem);
		avals.push_back(val);
	    }

	    RETVAL = THIS->write_entry(motif, avals);
	}
	OUTPUT: RETVAL
	    
