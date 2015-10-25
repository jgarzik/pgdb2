
#include <pgdb2.h>
#include <stdexcept>
#include <assert.h>

int main (int argc, char *argv[])
{
	pagedb::Options opts;
	opts.f_read = true;
	opts.f_write = false;
	opts.f_create = false;

	// TEST: fail, b/c foo.db does not exist
	bool saw_err = false;
	try {
		pagedb::DB db("foo.db", opts);
	}
	catch (const std::runtime_error& error) {
		saw_err = true;
	}
	catch (...) {
		assert(0);
	}
	assert(saw_err == true);

	// TEST: open/close foo.db
	opts.f_read = true;
	opts.f_write = true;
	opts.f_create = true;

	try {
		pagedb::DB db("foo.db", opts);
	}
	catch (...) {
		assert(0);
	}

	return 0;
}

