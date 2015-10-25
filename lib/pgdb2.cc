
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

DB::DB(std::string filename_, const Options& opt_)
{
	running = false;
	fd = -1;
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

	fd = ::open(filename.c_str(), flags, 0666);
	if (fd < 0)
		throw std::runtime_error("Failed open " + filename + ": " + strerror(errno));
}

void DB::readSuperblock()
{
	assert(fd >= 0);

	struct stat st;
	int frc = ::fstat(fd, &st);
	if (frc < 0)
		throw std::runtime_error("Failed fstat " + filename + ": " + strerror(errno));

	if ((st.st_size == 0) && (options.f_create)) {
		initSuperblock();
		writeSuperblock();
		return;
	}

	ssize_t rrc = ::read(fd, &sb, sizeof(sb));
	if (rrc < 0)
		throw std::runtime_error("Failed read " + filename + ": " + strerror(errno));
	if (rrc != sizeof(sb))
		throw std::runtime_error("Superblock short read");

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
}

void DB::initSuperblock()
{
	memset(&sb, 0, sizeof(sb));
	memcpy(sb.magic, SB_MAGIC, sizeof(sb.magic));
	sb.version = 1;
	sb.page_size = 4096;
	sb.features = SBF_MBO;
}

void DB::writeSuperblock()
{
	assert(fd >= 0);

	Superblock write_sb;
	memcpy(&write_sb, &sb, sizeof(sb));

	write_sb.version = htole32(write_sb.version);
	write_sb.page_size = htole32(write_sb.page_size);
	write_sb.features = htole64(write_sb.features);

	std::vector<unsigned char> page;
	page.resize(sb.page_size);
	memcpy(&page[0], &write_sb, sizeof(write_sb));

	off_t lres = ::lseek(fd, 0, SEEK_SET);
	if (lres < 0)
		throw std::runtime_error("Failed seek " + filename + ": " + strerror(errno));

	ssize_t wrc = ::write(fd, &page[0], page.size());
	if (wrc < 0)
		throw std::runtime_error("Failed write " + filename + ": " + strerror(errno));
	if (wrc != (ssize_t)page.size())
		throw std::runtime_error("Superblock short write");
}

DB::~DB()
{
	if (!running)
		return;
	
	close(fd);
	fd = -1;

	running = false;
}

} // namespace pagedb

