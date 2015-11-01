#ifndef __PGDB2_H__
#define __PGDB2_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <stdint.h>
#include <fcntl.h>
#include "pgdb2-file.h"

namespace pagedb {

#define SB_MAGIC "PGDB0000"

enum sb_features {
	SBF_MBO		= (1ULL << 63),		// must be one
	SBF_MBZ		= (1ULL << 62),		// must be zero
};

enum various_constants {
	INT_KEY_MAX	= 511,			// max key size before spill
};

struct Superblock {
	unsigned char	magic[8];		// file format unique id
	uint32_t	version;		// db version
	uint32_t	page_size;		// page size, in bytes
	uint64_t	features;		// feature bitmask
	uint64_t	inode_table_ref;	// page w/ list of ino tab pages
	uint64_t	reserved[64 - 4];
};

enum ext_flags {
	EF_MBO		= (1U << 31),		// must be one
	EF_MBZ		= (1U << 30),		// must be zero
	EF_HDR		= (1U << 29),		// header record
};

struct Extent {
	uint64_t	ext_page;		// extent page start
	uint32_t	ext_len;		// extent page count
	uint32_t	ext_flags;		// flags bitmask
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

	std::vector<Extent> inotab_ref;

public:
	DB(std::string filename_, const Options& opt_);
	~DB();

private:
	void open();

	void readSuperblock();
	void readExtList(std::vector<Extent> &ext_list);

	void writeSuperblock();
	void writeInotabRef();

	void clear();

};

} // namespace pagedb

#endif // __PGDB2_H__
