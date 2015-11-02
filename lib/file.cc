/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "pgdb2-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdexcept>
#include <vector>
#include <assert.h>
#include <pgdb2-file.h>

namespace pagedb {

File::File(std::string filename_, int o_flags_, size_t page_size_)
{
	fd = -1;
	o_flags = o_flags_;
	filename = filename_;
	page_size = page_size_;
}

File::~File()
{
	close();
}

void File::open()
{
	fd = ::open(filename.c_str(), o_flags, 0666);
	if (fd < 0)
		throw std::runtime_error("Failed open " + filename + ": " + strerror(errno));

	cur_fpos = 0;

	setPageCount();
}

void File::setPageCount()
{
	if (!isOpen())
		n_pages = 0;
	else {
		struct stat st;
		stat(st);

		n_pages = st.st_size / page_size;
	}
}

void File::setPageSize(size_t sz)
{
	page_size = sz;

	setPageCount();
}

void File::open(std::string filename_, int o_flags_, size_t page_size_)
{
	assert(fd < 0);

	filename = filename_;
	o_flags = o_flags_;
	page_size = page_size_;

	open();
}

void File::close()
{
	if (fd < 0)
		return;

	::close(fd);
	fd = -1;
}

void File::read(std::vector<unsigned char>& buf, uint64_t index,
		size_t page_count)
{
	off_t lrc = ::lseek(fd, index * page_size, SEEK_SET);
	if (lrc < 0)
		throw std::runtime_error("Failed seek " + filename + ": " + strerror(errno));

	size_t io_size = page_size * page_count;
	assert(buf.size() >= io_size);

	ssize_t rrc = ::read(fd, &buf[0], io_size);
	if (rrc < 0)
		throw std::runtime_error("Failed read " + filename + ": " + strerror(errno));
	if (rrc != (ssize_t)io_size)
		throw std::runtime_error("Short read");

	cur_fpos = lrc + io_size;
}

void File::write(const std::vector<unsigned char>& buf, uint64_t index,
		 size_t page_count)
{
	off_t lrc = ::lseek(fd, index * page_size, SEEK_SET);
	if (lrc < 0)
		throw std::runtime_error("Failed seek " + filename + ": " + strerror(errno));

	size_t io_size = page_size * page_count;
	assert(buf.size() >= io_size);

	ssize_t rrc = ::write(fd, &buf[0], io_size);
	if (rrc < 0)
		throw std::runtime_error("Failed write " + filename + ": " + strerror(errno));
	if (rrc != (ssize_t)io_size)
		throw std::runtime_error("Short write");

	cur_fpos = lrc + io_size;

	if ((index + page_count) > n_pages)
		n_pages = index + page_count;
}

void File::stat(struct stat& st)
{
	int frc = ::fstat(fd, &st);
	if (frc < 0)
		throw std::runtime_error("Failed fstat " + filename + ": " + strerror(errno));
}

void File::sync()
{
	int frc = ::fsync(fd);
	if (frc < 0)
		throw std::runtime_error("Failed fsync " + filename + ": " + strerror(errno));
}

} // namespace pagedb
