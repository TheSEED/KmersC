use Data::Dumper;
use KmersC;

my $cr = new KmersFileCreator(0xfeedface, 8, 3, [4,1]);
my $file = "/tmp/test1.$$.dat";
$cr->open_file($file);
$cr->write_file_header();
$cr->write_entry("abcdefghij",[1,2]);
$cr->write_entry("abcdffhigj",[3,4]);
$cr->close_file();
system("ls -l $file");
system("od -t x1 $file");

$k = new KmersC();
print "k=$k\n";
$k->open_data($file);
my $tstr= "xyzabcdefghijxafdabcdffhigjjasd";
my $l = [];
$res = $k->find_all_hits($tstr, $l);
print Dumper($l);
unlink $file;


