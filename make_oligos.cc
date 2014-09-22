/*
 * C++ version of the get_oligos code; simplified.
 * Takes as input a list of peg ids each with associated values,
 * as well as a berkeley btree of translations.
 * Walk the translations file in sorted order, and emit
 * the oligos for those which we are looking for based.
 * on the id list.
 */

#include <stdio.h>
#include <db.h>
#include <string>
#include <errno.h>
#include <map>
#include <vector>
#include <unistd.h>
#include <list>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

/*
 * Sort-behind write pipe.
 *
 * get_fp returns a filehandle for writing data.
 * The number of calls to get_fp is tracked; when it goes
 * over a limit, before a new fp is returned the previous one
 * is closed and the fid for its process put on a waiting
 * list. Each time through, the procs on the waiting
 * list are checked to see if they've finished.
 */
class sort_writer
{
public:
    sort_writer(const std::string &output_file_pattern);
    ~sort_writer();

    FILE *get_fp();

    FILE *cur_fp;
    int cur_pid;
    int file_count;

    void open_writer();
    void close_writer();
    void check_procs(bool wait);

    int write_limit;
    int write_count;

    std::string output_file_pattern;

    std::vector<int> waiting_pids;
};

sort_writer::sort_writer(const std::string &output_file_pattern) :
    output_file_pattern(output_file_pattern),
    write_limit(4000000),
    write_count(0),
    cur_fp(0),
    file_count(0)
{
    open_writer();
}

sort_writer::~sort_writer()
{
    close_writer();
    check_procs(true);
}

FILE *sort_writer::get_fp()
{
    if (write_count++ > write_limit)
    {
	check_procs(false);
	close_writer();
	open_writer();
	write_count = 0;
    }
    return cur_fp;
}

void sort_writer::open_writer()
{
    char outfile[1024];
    sprintf(outfile, output_file_pattern.c_str(), file_count++);
    
    int fd[2];
    pipe(fd);

    int rd = fd[0];
    int wr = fd[1];


    int pid = fork();
    if (pid < 0)
    {
	fprintf(stderr, "fork failed: %s\n", strerror(errno));
	exit(1);
    }
    else if (pid == 0)
    {
	int out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0664);
	if (out < 0)
	{
	    fprintf(stderr, "open %s failed: %s\n", outfile, strerror(errno));
	    exit(1);
	}

	dup2(rd, 0);
	dup2(out, 1);
	if (close(out) < 0)
	    perror("close out");
	    
	if (close(rd) < 0)
	    perror("close rd");

	if (close(wr) < 0)
	    perror("close wr");
	
	execlp("sort", "sort", "-S", "400M", 0);
	fprintf(stderr, "Exec failed: %s\n", strerror(errno));
	exit(1);
    }
    printf("Opened %s for %d rd=%d wr=%d\n", outfile, pid, rd, wr);
    cur_pid = pid;

    close(rd);

    fcntl(wr, F_SETFD, fcntl(wr, F_GETFD) | FD_CLOEXEC);
    
    cur_fp = fdopen(wr, "w");
}

void sort_writer::close_writer()
{
    printf("Close %d to %d\n", fileno(cur_fp), cur_pid);
    fclose(cur_fp);
    waiting_pids.push_back(cur_pid);
}

void sort_writer::check_procs(bool wait)
{
    /*
     * Check for old processes to see if they're done.
     *
     * We allow only one to remain running in the background.
     * n is the number of waiting pids; for pids 0.. n-2 we
     * pass flags=0. For pid n-1 we allow flags=WNOHANG if wait is false.
     */

    int n = waiting_pids.size();
    int i = 0;
    for (std::vector<int>::iterator it = waiting_pids.begin(); it != waiting_pids.end(); i++)
    {
	int rc;
	int flags;
	if (wait)
	{
	    flags = 0;
	}
	else if (i < n - 1)
	{
	    flags = 0;
	}
	else
	{
	    flags = WNOHANG;
	}
	    
	printf("Check on %d wait=%d i=%d n=%d flags=%d\n", *it, wait, i, n, flags);

	int d = waitpid(*it, &rc, flags);
	if (d < 0)
	{
	    fprintf(stderr, "Error on waidpid %d: %s\n", *it, strerror(errno));
	    ++it;
	}
	else if (d == 0)
	{
	    printf("%d not done yet\n", *it);
	    ++it;
	}
	else
	{
	    printf("%d finished (%d)\n", *it, d);
	    it = waiting_pids.erase(it);
	}
    }
}

typedef std::map<std::string, std::string > peg_to_value_map;

void open_output_pipes(std::string &out_dir, sort_writer *output_pipes[], char *chars);
void close_output_pipes(sort_writer *output_pipes[]);
int process_db(DB *db, peg_to_value_map &pegs, sort_writer *output_pipes[]);
void read_peg_map(FILE *fp, peg_to_value_map &pegs);

int kmin, kmax, emit_offsets;

int main(int argc, char **argv)
{
    if (argc != 6)
    {
	fprintf(stderr, "Usage: %s kmin kmax offset-flag translations-btree output_dir\n", argv[0]);
	exit(1);
    }

    kmin = atoi(argv[1]);
    kmax = atoi(argv[2]);
    printf("kmin=%d kmax=%d\n", kmin, kmax);

    emit_offsets = atoi(argv[3]);
    std::string trans_btree(argv[4]);
    std::string out_dir(argv[5]);

    /* Open btree */

    DB *db;
    if (int err = db_create(&db, 0, 0) != 0)
    {
	fprintf(stderr, "error creating db: %d\n", err);
	exit(1);
    }

    if (int err = db->open(db, 0, trans_btree.c_str(), 0, DB_BTREE, DB_RDONLY, 0644) != 0)
    {
	fprintf(stderr, "error opening db file %s: %d\n", trans_btree.c_str(), err);
	exit(1);
    }

    /*
     * Read stdin to collect list of data values
     */

    peg_to_value_map pegs;

    read_peg_map(stdin, pegs);

    sort_writer *output_pipes[256] = {0 };

    std::list<sort_writer *> writers;

    char *sets[] = { "ACDEFG", "HIKLMNP", "QRSTVWY", 0 };
//    char *sets[] = { "ACDEF", "GHIKL", "MNPQRS", "TVWY", 0 };

    for (int i = 0; sets[i] != 0; i++)
    {
	char *set = sets[i];
	char file[1024];

	sprintf(file, "%s/kmers.%s", out_dir.c_str(), set);
	std::string set_dir(file);

	if (mkdir(set_dir.c_str(), 0777) < 0 && errno != EEXIST)
	{
	    fprintf(stderr, "mkdir %s failed: %s\n", set_dir.c_str(), strerror(errno));
	    exit(1);
	}
	
	sprintf(file, "%s/%%05d", set_dir.c_str());
	sort_writer *writer = new sort_writer(file);

	for (char *sp = set; *sp != 0; sp++)
	    output_pipes[*sp] = writer;

	writers.push_back(writer);
    }
    
    //open_output_pipes(out_dir, output_pipes, "ACDEFGHIKLMNPQRSTVWY");

    process_db(db, pegs, output_pipes);

    db->close(db, 0);
    //    close_output_pipes(output_pipes);
    for (std::list<sort_writer *>::iterator it = writers.begin(); it != writers.end(); it++)
    {
	sort_writer *wr = *it;
	delete wr;
    }
}

void open_output_pipes(std::string &out_dir, sort_writer *output_pipes[], char *chars)
{
    char cmd[1024];
    for (char *c = chars; *c != 0; c++)
    {
	sprintf(cmd, "%s/kmers.%c.%%d", out_dir.c_str(), *c);
	output_pipes[*c] = new sort_writer(cmd);
	if (output_pipes[*c] == 0)
	{
	    fprintf(stderr, "error opening pipe %s: %s\n", cmd, strerror(errno));
	    exit(1);
	}
    }
}

void close_output_pipes(sort_writer *output_pipes[])
{
    for (int i = 0; i < 256; i++)
    {
	sort_writer *wr = output_pipes[i];
	if (wr)
	{
	    delete(wr);
	}
    }
}

void write_oligos(std::string &id, char *trans, int trans_len, std::string &value,
		  sort_writer *output_pipes[])
{
    char *s;
    int i;
    for (s = trans, i = 0; i < trans_len - kmin; i++, s++)
    {
	int olen = (i < (trans_len - kmax)) ? kmax : (trans_len - i);
	sort_writer *wr = output_pipes[*s];
	if (wr == 0)
	{
	    // fprintf(stderr, "No pipe for char %c (%s)\n", *s, id.c_str());
	    continue;
	}
	FILE *fp = wr->get_fp();
	fwrite(s, olen, 1, fp);
	fwrite("\t", 1, 1, fp);
	fwrite(value.c_str(), value.length(), 1, fp);
	if (emit_offsets)
	{
	    int offset = trans_len - i;
	    fprintf(fp, "\tOFF%d\n", offset);
	}
	else
	    fwrite("\n", 1, 1, fp);
    }

}


int process_db(DB *dbp, peg_to_value_map &pegs, sort_writer *output_pipes[])
{
    DBC *dbcp;
    DBT key, data;
    size_t retklen, retdlen;
    void *retkey, *retdata;
    void *p;
    int bufsiz = 1024 * 1024;
    int ret, t_ret;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    if ((data.data = malloc(bufsiz)) == NULL)
	return (errno);
    
    data.ulen = bufsiz;
    data.flags = DB_DBT_USERMEM;

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0))
	!= 0) {
	dbp->err(dbp, ret, "DB->cursor");
	free(data.data);
	return (ret);
    }

    int n = pegs.size();
    while (n > 0)
    {
	/*
	 * Acquire the next set of key/data pairs.  This code
	 * does not handle single key/data pairs that won't fit
	 * in a BUFFER_LENGTH size buffer, instead returning
	 * DB_BUFFER_SMALL to our caller.
	 */
	if ((ret = dbcp->c_get(dbcp,
			     &key, &data, DB_MULTIPLE_KEY | DB_NEXT)) != 0) {
	    if (ret != DB_NOTFOUND)
		dbp->err(dbp, ret, "DBcursor->get");
	    break;
	}

	for (DB_MULTIPLE_INIT(p, &data);;) {
	    DB_MULTIPLE_KEY_NEXT(p,
				 &data, retkey, retklen, retdata, retdlen);
	    if (p == NULL)
		break;

	    std::string id((char *)retkey, (int) retklen);

	    peg_to_value_map::iterator it = pegs.find(id);
	    if (it != pegs.end())
	    {
		std::string &v = it->second;
		write_oligos(id, (char *) retdata, (int) retdlen, v, output_pipes);
		n--;
	    }
	}
    }

    if ((t_ret = dbcp->c_close(dbcp)) != 0) {
	dbp->err(dbp, ret, "DBcursor->close");
	if (ret == 0)
	    ret = t_ret;
    }

    free(data.data);

    return (ret);
    
}

void read_peg_map(FILE *fp, peg_to_value_map &pegs)
{
    char buf[4096];

    while (fgets(buf, sizeof(buf), fp))
    {
	char *save;
	char *id = strtok_r(buf, "\t\n", &save);
	char *v = strtok_r(0, "\n", &save);
	pegs[id] = v;
    }
}
