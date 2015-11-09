#ifndef __PGDB2_FILE_H__
#define __PGDB2_FILE_H__
/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "pgdb2-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <stdint.h>
#include <fcntl.h>

namespace pagedb {

class File {
private:
	int fd;			// our file descriptor, or -1 (closed/invalid)
	int o_flags;		// flags passed to open(2)
	std::string filename;	// filename passed to open(2)

	size_t page_size;	// page size (short term: pow 2; LT: any)
	uint64_t n_pages;	// cache: current file size, in pages

	off_t cur_fpos;		// cache: current OS file position, or -1

public:
	File() : fd(-1), o_flags(0), page_size(4096), n_pages(0), cur_fpos(-1) {}
	File(const std::string& filename_, int o_flags_ = O_RDONLY, size_t page_size = 4096);
	~File();

	int fileno() const { return fd; }
	bool isOpen() const { return (fd >= 0); }
	uint64_t size() const { return n_pages; }
	size_t pageSize() const { return page_size; }
	void setPageSize(size_t sz);

	void open();
	void open(std::string filename_, int o_flags_ = O_RDONLY, size_t page_size = 4096);
	void close();

	void read(void *buf, uint64_t index, size_t page_count = 1);
	void read(std::vector<unsigned char>& buf, uint64_t index, size_t page_count = 1);

	void write(const void *buf, uint64_t index, size_t page_count = 1);
	void write(const std::vector<unsigned char>& buf, uint64_t index, size_t page_count = 1);

	void sync();
	void resize(uint64_t page_count);
	void extend(uint64_t deltaPages);

private:
	void setPageCount();
	void stat(struct stat& st);
};

static inline void bufSizeAlign(std::vector<unsigned char>& buf, size_t page_size) {
	size_t rem = buf.size() % page_size;
	if (rem || (buf.size() == 0))
		buf.resize(buf.size() + (page_size - rem));
}

} // namespace pagedb

#endif // __PGDB2_FILE_H__
