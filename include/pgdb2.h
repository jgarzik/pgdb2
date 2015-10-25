#ifndef __PGDB2_H__
#define __PGDB2_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <stdint.h>
#include <fcntl.h>

namespace pagedb {

#define SB_MAGIC "PGDB0000"

enum sb_features {
	SBF_MBO		= (1ULL << 63),		// must be one
	SBF_MBZ		= (1ULL << 62),		// must be zero
};

struct Superblock {
	unsigned char	magic[8];
	uint32_t	version;
	uint32_t	page_size;
	uint64_t	features;
	uint64_t	reserved[64 - 3];
};

class File {
private:
	int fd;
	int o_flags;
	std::string filename;
	size_t page_size;
	off_t cur_fpos;

public:

	File() : fd(-1), o_flags(0), page_size(4096), cur_fpos(-1) {}
	File(std::string filename_, int o_flags_ = O_RDONLY, size_t page_size = 4096);
	~File();

	bool isOpen() { return (fd >= 0); }
	void setPageSize(size_t sz) { page_size = sz; }

	void open();
	void open(std::string filename_, int o_flags_ = O_RDONLY, size_t page_size = 4096);
	void close();
	void read(uint64_t index, std::vector<unsigned char>& buf, size_t page_count = 1);
	void write(uint64_t index, const std::vector<unsigned char>& buf, size_t page_count = 1);
	void stat(struct stat& st);
};

class Options {
public:
	bool		f_read;
	bool		f_write;
	bool		f_create;

	Options() : f_read(true), f_write(false), f_create(false) {}
};

class DB {
private:
	bool		running;

	std::string	filename;
	Options		options;

	File		f;
	Superblock	sb;

public:
	DB(std::string filename_, const Options& opt_);
	~DB();

private:
	void open();
	void readSuperblock();
	void writeSuperblock();
	void initSuperblock();

};

} // namespace pagedb

#endif // __PGDB2_H__
