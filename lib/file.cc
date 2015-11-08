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

#ifdef HAVE_POSIX_FADVISE
	if (::posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM) < 0)
		throw std::runtime_error("Failed fadvise " + filename + ": " + strerror(errno));
#endif

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

void File::read(void *buf, uint64_t index, size_t page_count)
{
	if ((index + page_count) > n_pages)
		throw std::runtime_error("Read past EOF");

	// seek to page
	off_t lrc = ::lseek(fd, index * page_size, SEEK_SET);
	if (lrc < 0)
		throw std::runtime_error("Failed seek " + filename + ": " + strerror(errno));

	size_t io_size = page_size * page_count;

	// begin I/O
	ssize_t rrc = ::read(fd, buf, io_size);
	if (rrc < 0)
		throw std::runtime_error("Failed read " + filename + ": " + strerror(errno));
	if (rrc != (ssize_t)io_size)
		throw std::runtime_error("Short read");

	cur_fpos = lrc + io_size;
}

void File::read(std::vector<unsigned char>& buf_vec, uint64_t index,
		size_t page_count)
{
	size_t io_size = page_size * page_count;
	if (buf_vec.size() < io_size)
		buf_vec.resize(io_size);

	void *buf = &buf_vec[0];
	read(buf, index, page_count);
}

void File::write(const void *buf, uint64_t index, size_t page_count)
{
	// seek to page, if not already there
	off_t lrc = cur_fpos;
	off_t want_fpos = index * page_size;
	if (want_fpos != lrc) {
		lrc = ::lseek(fd, want_fpos, SEEK_SET);
		if (lrc < 0)
			throw std::runtime_error("Failed seek " + filename + ": " + strerror(errno));
	}

	size_t io_size = page_size * page_count;

	// begin I/O
	ssize_t rrc = ::write(fd, buf, io_size);
	if (rrc < 0)
		throw std::runtime_error("Failed write " + filename + ": " + strerror(errno));
	if (rrc != (ssize_t)io_size)
		throw std::runtime_error("Short write");

	// update cached file position, file size
	cur_fpos = lrc + io_size;

	if ((index + page_count) > n_pages)
		n_pages = index + page_count;
}

void File::write(const std::vector<unsigned char>& buf_vec, uint64_t index,
		 size_t page_count)
{
	size_t io_size = page_size * page_count;
	assert(buf_vec.size() >= io_size);

	const void *buf = &buf_vec[0];
	write(buf, index, page_count);
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

void File::resize(uint64_t page_count)
{
	if (page_count == n_pages)
		return;

	// extend OS file with zeroes
	else if (page_count > n_pages) {
		uint64_t n_alloc = page_count - n_pages;
		uint64_t start_idx = n_pages;
		uint64_t end_idx = n_pages + n_alloc;

		std::vector<unsigned char> zero_buf(page_size);
		for (uint64_t idx = start_idx; idx < end_idx; idx++)
			write(zero_buf, idx);

		// TODO: posix_fallocate()
	}

	// shrink OS file
	else if (page_count < n_pages) {
		off_t new_size = page_count * page_size;
		if (::ftruncate(fd, new_size) < 0)
			throw std::runtime_error("Failed ftruncate " + filename + ": " + strerror(errno));

		n_pages = page_count;

		if (cur_fpos > new_size) {
			off_t lrc = ::lseek(fd, 0, SEEK_SET);
			if (lrc < 0)
				throw std::runtime_error("Failed seek " + filename + ": " + strerror(errno));
			cur_fpos = 0;
		}
	}

	// full sync to update OS filesystem inode, directory etc.
	sync();
}

static uint64_t getFileIncrement(uint64_t size)
{
	if (size > 16384)
		return 16384;
	if (size > 1024)
		return 1024;
	if (size > 256)
		return 256;
	return 64;
}

void File::extend(uint64_t deltaPages)
{
	// round file size to next increment
	uint64_t min_size = n_pages + deltaPages;
	uint64_t slab_size = getFileIncrement(min_size);
	uint64_t n_slabs = min_size / slab_size;
	if (min_size % slab_size)
		n_slabs++;

	uint64_t new_size = n_slabs * slab_size;

	// resize OS file + fsync
	resize(new_size);
}

} // namespace pagedb
