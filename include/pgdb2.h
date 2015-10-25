#ifndef __PGDB2_H__
#define __PGDB2_H__

#include <string>
#include <stdint.h>

namespace pagedb {

struct Superblock {
	unsigned char	magic[8];
	uint32_t	version;
	uint32_t	page_size;
	uint64_t	features;
	uint64_t	reserved[64 - 3];
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
	int		fd;
	std::string	filename;
	Options		options;
	Superblock	sb;

public:
	DB(std::string filename_, const Options& opt_);
	~DB();

private:
	void open();
	void readSuperblock();
	void initSuperblock();

};

} // namespace pagedb

#endif // __PGDB2_H__
