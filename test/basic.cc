/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "pgdb2-config.h"

#include <pgdb2.h>
#include <stdexcept>
#include <assert.h>
#include <iostream>

int main (int argc, char *argv[])
{
	page::Options opts;
	opts.f_read = true;
	opts.f_write = false;
	opts.f_create = false;

	// TEST: fail, b/c foo.db does not exist
	bool saw_err = false;
	try {
		page::DB db("foo.db", opts);
	}
	catch (const std::runtime_error& error) {
		saw_err = true;
	}
	catch (...) {
		assert(0);
	}
	assert(saw_err == true);

	// TEST: open/close new foo.db
	opts.f_read = true;
	opts.f_write = true;
	opts.f_create = true;

	try {
		page::DB db("foo.db", opts);
	}
	catch (const std::runtime_error& error) {
		std::cerr << error.what() << "\n";
		assert(0);
	}
	catch (...) {
		assert(0);
	}

	// TEST: open/close pre-existing foo.db, read-only
	opts.f_read = true;
	opts.f_write = false;
	opts.f_create = false;

	try {
		page::DB db("foo.db", opts);
	}
	catch (const std::runtime_error& error) {
		std::cerr << error.what() << "\n";
		assert(0);
	}
	catch (...) {
		assert(0);
	}

	// TEST: open/close pre-existing foo.db, read/write
	opts.f_read = true;
	opts.f_write = true;
	opts.f_create = false;

	try {
		page::DB db("foo.db", opts);
	}
	catch (const std::runtime_error& error) {
		std::cerr << error.what() << "\n";
		assert(0);
	}
	catch (...) {
		assert(0);
	}

	return 0;
}

