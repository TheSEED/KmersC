
#include <stdio.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <boost/format.hpp>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <strings.h>
#include <zlib.h>

class uniq_reader
{
public:
    uniq_reader(const std::string &file);
    ~uniq_reader();
    char *getline();
    char *this_line;
    char *prev_line;
    int bufsiz;
    gzFile gzfp;
    FILE *fp;
private:
    void _close();
    char *_getline(char *, int);
};

uniq_reader::uniq_reader(const std::string &file)
{
    bufsiz = 4096;
    prev_line = new char[bufsiz];
    this_line = new char[bufsiz];

    if (file == "-")
    {
	fp = stdin;
	gzfp = 0;
    }
    else
    {
	fp = 0;
	gzfp = gzopen(file.c_str(), "r");
	if (gzfp == 0)
	{
	    fprintf(stderr, "Error opening input %s: %s\n", file.c_str(), strerror(errno));
	    exit(1);
	}
    }
    
    if (_getline(prev_line, bufsiz) == 0)
    {
	_close();
	return;
    }
}

char *uniq_reader::_getline(char *line, int len)
{
    if (fp)
    {
	return fgets(line, len, fp);
    }
    else
    {
	return gzgets(gzfp, line, len);
    }
}

void uniq_reader::_close()
{
    if (fp)
    {
	fclose(fp);
	fp = 0;
    }
    else if (gzfp)
    {
	gzclose(gzfp);
	gzfp = 0;
    }
}

uniq_reader::~uniq_reader()
{
    _close();
    if (prev_line)
	delete [] prev_line;
    delete [] this_line;
}

/*
 * Read ahead until we get a line that
 * is different that prev_line that is kept in the obj.
 *
 * When we get a new line, save prev_line, set
 * prev_line to the new line, and return the saved
 * prev_line to the caller.
 *
 * When we reach EOF, save prev_line, set prev_line
 * to null (and deallocate), and return saved line
 * to the caller.
 *
 * If we are called when prev_line is null, return null.
 */
char *uniq_reader::getline()
{
    if (prev_line == 0)
	return 0;
    
    while (_getline(this_line, bufsiz) != 0)
    {
	if (strcmp(this_line, prev_line) != 0)
	{
	    char *t = prev_line;
	    prev_line = this_line;
	    this_line = t;
	    return this_line;
	}
    }
    char *t = prev_line;
    prev_line = this_line;
    this_line = t;
    delete [] prev_line;
    prev_line = 0;
    return this_line;
}

int parse_line_2_col(char *line, std::string &motif, int &v1, int &v2)
{
    char *save;
    char *m = strtok_r(line, "\t\n", &save);
    motif.assign(m);
    char *v = strtok_r(0, "\t\n", &save);
    if (v == 0)
	return 0;
    v1 = atoi(v);
    while (v = strtok_r(0, "\t\n", &save))
    {
	if (strncmp(v, "OFF", 3) == 0)
	{
	    v2 = atoi(v + 3);
	    break;
	}
    }
    return 1;
}

int parse_line_3_col(char *line, std::string &motif, int &v1, int &v2)
{
    char *save;
    char *m = strtok_r(line, "\t\n", &save);
    motif.assign(m);
    char *v = strtok_r(0, "\t\n", &save);
    if (v == 0)
	return 0;
    v = strtok_r(0, "\t\n", &save);
    if (v == 0)
	return 0;
    if (strncmp(v, "FIG", 3) == 0)
	v1 = atoi(v + 3);
    else
	v1 = atoi(v);
    
    while (v = strtok_r(0, "\t\n", &save))
    {
	if (strncmp(v, "OFF", 3) == 0)
	{
	    v2 = atoi(v + 3);
	    break;
	}
    }
    return 1;
}

typedef int (*parse_fn)(char *, std::string &, int &, int&);
typedef std::vector<gzFile> open_file_list;
void main_loop(uniq_reader &, parse_fn parser, open_file_list &output_pipes, int kmin, int kmax);


int main(int argc, char **argv)
{
    if (argc != 5)
    {
	fprintf(stderr, "Usage: %s outdir range value-col input-file\n", argv[0]);
	exit(1);
    }

    std::string outdir = argv[1];

    int kmin, kmax;

    kmin = atoi(argv[2]);

    if (char *s = index(argv[2], '-'))
	kmax = atoi(s + 1);
    else
	kmax = kmin;
    printf("got kmin=%d kmax=%d\n", kmin, kmax);

    int value_column = atoi(argv[3]);
    parse_fn column_parser;
    if (value_column == 2)
	column_parser = parse_line_2_col;
    else if (value_column == 3)
	column_parser = parse_line_3_col;
    else
    {
	fprintf(stderr, "value column must only be 2 or 3\n");
	exit(1);
    }

    /* Open input. */

    uniq_reader reader(argv[4]);

    /* Open output pipes to gzip. */

    if (mkdir(outdir.c_str(), 0777) < 0 && errno != EEXIST)
    {
	fprintf(stderr, "error creating %s: %s\n", outdir.c_str(), strerror(errno));
	exit(1);
    }
    
    open_file_list output_pipes(kmax + 1);
    for (int k = kmin; k <= kmax; k++)
    {
	std::string dir = str(boost::format("%1%/%2%") % outdir % k);
	if (mkdir(dir.c_str(), 0777) < 0 && errno != EEXIST)
	{
	    fprintf(stderr, "error creating %s: %s\n", dir.c_str(), strerror(errno));
	    exit(1);
	}
	
	std::string cmd = str(boost::format("%1%/good.oligos.gz") % dir);

	gzFile fp = gzopen(cmd.c_str(), "w");
	printf("cmd='%s'\n", cmd.c_str());
	if (fp == 0)
	{
	    fprintf(stderr, "Error opening gz to %s: %s\n", cmd.c_str(), strerror(errno));
	    exit(1);
	}
	output_pipes[k] = fp;
    }

    main_loop(reader, column_parser, output_pipes, kmin, kmax);

    for (int k = kmin; k <= kmax; k++)
    {
	gzFile fp = output_pipes[k];
	if (int rc = gzclose(fp) < 0)
	{
	    printf("Error closing k=%d pipe: rc=%d err=%s\n", k, rc, strerror(errno));
	}
    }
}

struct datum
{
    std::string motif;
    int func_index;
    int offset;
};
typedef std::list<datum> datum_list;

void process_set(datum_list &set, open_file_list &output_pipes, int kmin, int kmax);

void main_loop(uniq_reader &reader, parse_fn parser, open_file_list &output_pipes, int kmin, int kmax)
{
    std::string cur_motif;

    char *line = reader.getline();

    while (line)
    {
	std::string motif;
	int v1, v2;
	parser(line, motif, v1, v2);

	std::string cur_motif = motif.substr(0, kmin);
	datum_list set;

	while (line)
	{
	    parser(line, motif, v1, v2);
	    std::string sub = motif.substr(0, kmin);
	    
	    if (sub != cur_motif)
		break;

	    datum d = { motif, v1, v2 };
	    set.push_back(d);
	    line = reader.getline();
	}
	process_set(set, output_pipes, kmin, kmax);
    }
}

struct tuple
{
    int accum;
    int count;
    int max;
    int min;
};
typedef std::map<int, tuple> accum_map;

typedef std::map<int, int> count_map;
typedef std::map<std::string, count_map> oligo_count_map;

void process_set(datum_list &set, open_file_list &output_pipes, int kmin, int kmax)
{
    for (int k = kmin; k <= kmax; k++)
    {
	gzFile fp = output_pipes[k];
	accum_map sums;
	oligo_count_map counts;
	for (datum_list::iterator it = set.begin(); it != set.end(); it++)
	{
	    std::string m = it->motif;

	    if (m.length() < k)
		continue;
	    
	    int func_index  = it->func_index;
	    int offset = it->offset;

	    std::string short_oligo = m.substr(0, k);
	    accum_map::iterator it = sums.find(func_index);
	    if (it == sums.end())
	    {
		tuple t = { offset, 1, offset, offset };
		sums[func_index] = t;
	    }
	    else
	    {
		it->second.accum += offset;
		it->second.count++;
		if (offset > it->second.max)
		    it->second.max = offset;
		if (offset < it->second.min)
		    it->second.min = offset;
	    }

	    count_map &cm = counts[short_oligo];
	    cm[func_index]++;
	}

	for (oligo_count_map::iterator it = counts.begin(); it != counts.end(); it++)
	{
	    const std::string &oligo = it->first;
	    count_map &m = it->second;

	    count_map::iterator cit = m.begin();
	    if (cit != m.end())
	    {
		int sum = cit->second;
		int max = cit->first;
		int maxv = cit->second;
		
		for (cit++; cit != m.end(); cit++)
		{
		    int func = cit->first;
		    int count = cit->second;
		    if (count > maxv)
		    {
			maxv = count;
			max = func;
		    }
		    sum += count;
		}

		if (maxv == sum)
		{
		    accum_map::iterator tup = sums.find(max);
		    int fn_total = 0, fn_count = 0, fn_max = 0, fn_min = 0,avg = 0;
		    if (tup != sums.end())
		    {
			fn_total = tup->second.accum;
			fn_count = tup->second.count;
			fn_max = tup->second.max;
			fn_min = tup->second.min;
			if (fn_count > 0)
			    avg = fn_total / fn_count;
		    }

		    gzprintf(fp, "%s\t%d\t%d\t%d\t%d\n", oligo.c_str(), max, avg, fn_max, fn_min);
		}
	    }
	}
	
	    
    }
}
