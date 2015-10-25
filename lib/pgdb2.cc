
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdexcept>
#include <vector>
#include <assert.h>
#include <pgdb2.h>
#include <endian_compat.h>

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

void File::read(uint64_t index, std::vector<unsigned char>& buf,
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

void File::write(uint64_t index, const std::vector<unsigned char>& buf,
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
}

void File::stat(struct stat& st)
{
	int frc = ::fstat(fd, &st);
	if (frc < 0)
		throw std::runtime_error("Failed fstat " + filename + ": " + strerror(errno));
}

DB::DB(std::string filename_, const Options& opt_)
{
	running = false;

	filename = filename_;
	options = opt_;

	open();
	readSuperblock();

	running = true;
}

void DB::open()
{
	int flags = 0;

	if (options.f_read && options.f_write)
		flags |= O_RDWR;
	else if (options.f_read)
		flags |= O_RDONLY;
	else
		throw std::runtime_error("Invalid read/write options");

	if (options.f_create) {
		if (!options.f_write)
			throw std::runtime_error("Invalid creat/write options");
		flags |= O_CREAT;
	}

	f.open(filename, flags, sizeof(Superblock));
}

void DB::readSuperblock()
{
	assert(f.isOpen());

	struct stat st;
	f.stat(st);

	if ((st.st_size == 0) && (options.f_create)) {
		initSuperblock();
		writeSuperblock();
		return;
	}

	std::vector<unsigned char> sb_buf(4096);
	f.read(0, sb_buf);

	memcpy(&sb, &sb_buf[0], sizeof(sb));

	sb.version = le32toh(sb.version);
	sb.page_size = le32toh(sb.page_size);
	sb.features = le64toh(sb.features);

	if (memcmp(sb.magic, SB_MAGIC, sizeof(sb.magic)))
		throw std::runtime_error("Superblock invalid magic");
	if (sb.version < 1)
		throw std::runtime_error("Superblock invalid version");
	if (sb.page_size < 512 || sb.page_size > 65536)
		throw std::runtime_error("Superblock invalid page size");
	if ((!(sb.features & SBF_MBO)) || (sb.features & SBF_MBZ))
		throw std::runtime_error("Superblock invalid features");

	f.setPageSize(sb.page_size);
}

void DB::initSuperblock()
{
	memset(&sb, 0, sizeof(sb));
	memcpy(sb.magic, SB_MAGIC, sizeof(sb.magic));
	sb.version = 1;
	sb.page_size = 4096;
	sb.features = SBF_MBO;

	f.setPageSize(sb.page_size);
}

void DB::writeSuperblock()
{
	assert(f.isOpen());

	Superblock write_sb;
	memcpy(&write_sb, &sb, sizeof(sb));

	write_sb.version = htole32(write_sb.version);
	write_sb.page_size = htole32(write_sb.page_size);
	write_sb.features = htole64(write_sb.features);

	std::vector<unsigned char> page;
	page.resize(sb.page_size);
	memcpy(&page[0], &write_sb, sizeof(write_sb));

	f.write(0, page);
}

DB::~DB()
{
	if (!running)
		return;

	running = false;
}

} // namespace pagedb

