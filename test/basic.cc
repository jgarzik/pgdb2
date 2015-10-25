
#include <pgdb2.h>

int main (int argc, char *argv[])
{
	pagedb::Options opts;
	opts.f_read = true;
	opts.f_write = true;
	opts.f_create = true;

	pagedb::DB db("foo.db", opts);

	return 0;
}

