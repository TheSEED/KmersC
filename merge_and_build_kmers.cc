/*
 * Merge 3 sets of files; each has lines of the form
 *
 *    oligo data-value
 *
 * We are hard-coding at this time the columns that
 * data is taken from and the columns in the output.
 * 
 * The three set of files are
 *
 *    Figfam - function data. Column 2 goes to column 1; column 3 goes to column 4.
 *    Figfam - figfam data. Column 2 goes to column 3.
 *    Phylo data. Column 2 goes to column 2.
 *
 *
 * This program takes command arguments as follows:
 *
 *    Figfam-function data file. Contains list of file names in sorted order that
 *    source the figfam-function data.
 *
 *    Figfam-figfam data file. As above.
 *
 *    Phylo data file. As above.
 *
 *    Kmer size.
 *
 *    Output file. 
 */

#include <stdlib.h>
#include <string>
#include <vector>
#include <list>
#include <utility>
#include <algorithm>
#include <iostream>
#include <fstream>
#include "kmers.h"
#include <sys/stat.h>
#include <zlib.h>
#include <errno.h>

typedef std::vector<std::pair<int, int> > pairlist;

class FileSource
{
public:
    FileSource(const std::string &file) : file(file) {};
    virtual ~FileSource() {};
    virtual char *get_line(char *l, int len) = 0;

    std::string file;
};

class RawFileSource : public FileSource
{
public:
    RawFileSource(const std::string &file) : FileSource(file)
    {
	fp = fopen(file.c_str(), "r");
	if (fp == 0)
	{
	    fprintf(stderr, "Error opening %s: %s\n", file.c_str(), strerror(errno));
	    exit(1);
	}
    };
    ~RawFileSource()
    {
	fclose(fp);
    };

    virtual char *get_line(char *s, int len)
    {
	return fgets(s, len, fp);
    };

    FILE *fp;
};

class GzipFileSource : public FileSource
{
public:
    GzipFileSource(const std::string &file) : FileSource(file)
    {
	char cmd[1024];
	sprintf(cmd, "gunzip -c %s", file.c_str());
	printf("Pipe from %s\n", cmd);
	fp = popen(cmd, "r");
	if (fp == 0)
	{
	    fprintf(stderr, "Error opening %s: %s\n", cmd, strerror(errno));
	    exit(1);
	}
    };
    ~GzipFileSource()
    {
	pclose(fp);
    };

    virtual char *get_line(char *str, int len)
    {
	return fgets(str, len, fp);
    };

    FILE *fp;
};

class DataReader
{
public:
    DataReader(int id, const std::string &file, const pairlist &pairs);
    ~DataReader() {
	//if (cur_input) gzclose(cur_input);
	if (cur_input) delete cur_input;
    }
    std::string cur;
    std::vector<int> cur_value;
    bool move_next();
    void write_values(std::vector<int> &target);
    void write_values(int target[]);

    int id;

private:
    std::string files_to_read;
    std::list<std::string> input_files;
//    gzFile cur_input;
    FileSource * cur_input;
    char cur_line[4096];
    pairlist pairs;
    void open_input();
};

bool min_reader(DataReader *d1, DataReader *d2)
{
    bool x = d1->cur < d2->cur;
    return x;
}


DataReader::DataReader(int id, const std::string &file, const pairlist &pairs) :
    id(id), files_to_read(file), pairs(pairs), cur_value(10,0)
{
    printf("Create datareader %d %s\n", id, files_to_read.c_str());

    std::ifstream xstr(files_to_read.c_str(), std::ifstream::in);
    char buf[4096];
    while (xstr.getline(buf, sizeof(buf), '\n'))
    {
	struct stat s;
	if (stat(buf, &s) < 0)
	{
	    fprintf(stderr, "No such file '%s'\n", buf);
	    exit(1);
	}
	printf("Got file %s\n", buf);
	input_files.push_back(std::string(buf));
    }

    open_input();
    move_next();
}

void DataReader::open_input()
{
    if (input_files.size() == 0)
    {
	/* Special case for empty input since zcat will bail. */
	if (cur_input)
	{
	    //gzclose(cur_input);
	    delete cur_input;
	    cur_input = 0;
	}
	return;
    }

    std::string next_inp = input_files.front();
    input_files.pop_front();

    printf("Opening '%s', left=%d\n", next_inp.c_str(), input_files.size());

    size_t found = next_inp.find_last_of(".");
    if (next_inp.substr(found + 1) == "gz")
    {
	cur_input = new GzipFileSource(next_inp);
    }
    else
    {
	cur_input = new RawFileSource(next_inp);
    }
    
    //cur_input = gzopen(next_inp.c_str(), "r");
}

bool DataReader::move_next()
{
//    printf("move %d\n", id);

    if (cur_input == 0)
	return false;
	
    if (cur_input->get_line(cur_line, sizeof(cur_line)))
//    if (gzgets(cur_input, cur_line, sizeof(cur_line)))
    {
//	printf("read %d: %s", id, cur_line);
	char *save = 0;
	char *x = strtok_r(cur_line, "\t\n", &save);
	cur = x;
	x = strtok_r(0, "\t\n", &save);
	int i = 0;
	while (x)
	{
	    int val = atoi(x);
	    cur_value[i++] = val;
	    x = strtok_r(0, "\t\n", &save);
	}
//	printf("vec %d %d %d %d\n", cur_value[0], cur_value[1], cur_value[2], cur_value[3]);
	return true;
    }
    else
    {
	//gzclose(cur_input);
	delete cur_input;
	cur_input = 0;
	if (input_files.size() == 0)
	{
	    return false;
	}
	else
	{
	    open_input();
	    return true;
	}
    }
}

void DataReader::write_values(std::vector<int> &target)
{
    for (pairlist::iterator it = pairs.begin(); it != pairs.end(); it++)
    {
	int from = it->first;
	int to = it->second;
	target[to] = cur_value[from];
//	printf("Assigned %d => %d (%d %d)\n", from, to, cur_value[from], target[to]);
    }
}

void DataReader::write_values(int target[])
{
    for (pairlist::iterator it = pairs.begin(); it != pairs.end(); it++)
    {
	int from = it->first;
	int to = it->second;
	target[to] = cur_value[from];
//	printf("Assigned %d => %d (%d %d)\n", from, to, cur_value[from], target[to]);
    }
}

typedef std::vector<DataReader *> reader_list;

void read_files(reader_list &readers, int kmer, const std::string &outfile)
{
    std::vector<int> attr_len;
    attr_len.push_back(4);
    attr_len.push_back(2);
    attr_len.push_back(4);
    attr_len.push_back(4);
    KmersFileCreator creator(0xfeedface, kmer, 0, attr_len);

    creator.open_file((char *) outfile.c_str());
    creator.write_file_header();
    
    std::string cur_motif("");
//    int empty_values[4] = { -1, -1, -1, -1 };
//    int values[4];
    std::vector<int> values(4, -1);
    
//    memcpy(values, empty_values, sizeof(empty_values));
    while (!readers.empty())
    {
	reader_list::iterator mr = std::min_element(readers.begin(), readers.end(), min_reader);

	DataReader *m = *mr;

//	printf("reader %d m='%s' cur='%s' %d %d %d %d\n", m->id, m->cur.c_str(), cur_motif.c_str(), m->cur_value[0], m->cur_value[1], m->cur_value[2], m->cur_value[3]);

	if (m->cur != cur_motif)
	{
	    if (cur_motif != "")
	    {
//		printf("Writing %s %d %d %d %d\n", cur_motif.c_str(),
//		       values[0], values[1], values[2], values[3]);
		if (values[0] >= 0)
		    creator.write_entry((char *) cur_motif.c_str(), values);
		//memcpy(values, empty_values, sizeof(empty_values));
		values.assign(4, -1);
	    }

	    cur_motif = m->cur;
	}
	m->write_values(values);
	bool ok = m->move_next();
	if (!ok)
	{
	    readers.erase(mr);
	}
    }
    creator.write_entry((char *) cur_motif.c_str(), values);
    creator.close_file();
}

int main(int argc, char **argv)
{
    if (argc != 6)
    {
	fprintf(stderr, "Usage: %s ff-funcdata ff-ffdata phylo-data kmer-size output-file\n", argv[0]);
	exit(1);
    }

    pairlist p1, p2, p3;

    p1.push_back(std::make_pair(0,0));
    p1.push_back(std::make_pair(2,3));

    DataReader funcdata(1, argv[1], p1);
    
    p2.push_back(std::make_pair(0,2));

    DataReader ffdata(2, argv[2], p2);
    
    p3.push_back(std::make_pair(0,1));

    DataReader phylodata(3, argv[3], p3);

    int kmer = atoi(argv[4]);

    std::string outfile(argv[5]);

    reader_list readers;

    readers.push_back(&funcdata);
    readers.push_back(&ffdata);
    readers.push_back(&phylodata);

    read_files(readers, kmer, outfile);
}
 
