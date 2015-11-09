/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "pgdb2-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <pgdb2.h>

#define TESTFN "file.db"

static void test1()
{
	// TEST: fail to open non-existent file
	bool saw_err = false;
	try {
		page::File f;
		f.open("does-not-exist");
	}
	catch (const std::runtime_error& error) {
		saw_err = true;
	}
	catch (...) {
		assert(0);
	}
	assert(saw_err == true);
}

static void test2()
{
	bool saw_err;
	page::File f;

	// TEST: basic file operations

	// open new file
	try {
		f.open(TESTFN, O_RDWR | O_CREAT | O_TRUNC);
	}
	catch (...) {
		assert(0);
	}

	assert(f.fileno() >= 0);
	assert(f.isOpen() == true);
	assert(f.size() == 0);
	assert(f.pageSize() == 4096);

	struct stat st;
	assert(stat(TESTFN, &st) == 0);
	assert(st.st_size == 0);

	// write page to file
	std::vector<unsigned char> buf(f.pageSize());
	try {
		f.write(buf, 0, 1);
	}
	catch (...) {
		assert(0);
	}

	assert(f.size() == 1);

	// sync data to storage
	try {
		f.sync();
	}
	catch (...) {
		assert(0);
	}

	// read page from file
	std::vector<unsigned char> buf2;
	try {
		f.read(buf2, 0, 1);
	}
	catch (...) {
		assert(0);
	}

	assert(buf2.size() == buf.size());
	for (unsigned int i = 0; i < buf.size(); i++)
		assert(buf[i] == buf2[i]);

	// extend file + resize(grow)
	try {
		f.extend(20);
	}
	catch (...) {
		assert(0);
	}

	// extend() rounds up to 64, as the first size increment
	assert(f.size() == 64);
	assert(stat(TESTFN, &st) == 0);
	assert(st.st_size == (off_t)(64 * f.pageSize()));

	// resize(shrink)
	try {
		f.resize(32);
	}
	catch (...) {
		assert(0);
	}

	assert(f.size() == 32);
	assert(stat(TESTFN, &st) == 0);
	assert(st.st_size == (off_t)(32 * f.pageSize()));

	// read past EOF
	saw_err = false;
	try {
		f.read(buf, 32, 1);
	}
	catch (const std::runtime_error& error) {
		saw_err = true;
	}
	catch (...) {
		assert(0);
	}
	assert(saw_err == true);

	// close
	try {
		f.close();
	}
	catch (...) {
		assert(0);
	}

	assert(unlink(TESTFN) == 0);
}

static void runtests()
{
	test1();
	test2();
}

int main (int argc, char *argv[])
{
	runtests();
	return 0;
}

