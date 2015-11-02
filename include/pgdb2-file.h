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
	int fd;
	int o_flags;
	std::string filename;
	size_t page_size;
	off_t cur_fpos;
	uint64_t n_pages;

public:

	File() : fd(-1), o_flags(0), page_size(4096), cur_fpos(-1) {}
	File(std::string filename_, int o_flags_ = O_RDONLY, size_t page_size = 4096);
	~File();

	bool isOpen() const { return (fd >= 0); }
	uint64_t size() const { return n_pages; }
	size_t pageSize() const { return page_size; }
	void setPageSize(size_t sz);

	void open();
	void open(std::string filename_, int o_flags_ = O_RDONLY, size_t page_size = 4096);
	void close();
	void read(std::vector<unsigned char>& buf, uint64_t index, size_t page_count = 1);
	void write(const std::vector<unsigned char>& buf, uint64_t index, size_t page_count = 1);
	void sync();

private:
	void setPageCount();
	void stat(struct stat& st);
};

} // namespace pagedb

#endif // __PGDB2_FILE_H__
