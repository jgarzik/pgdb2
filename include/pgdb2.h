#ifndef __PGDB2_H__
#define __PGDB2_H__
/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "pgdb2-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <stdexcept>
#include <vector>
#include <stdint.h>
#include <fcntl.h>
#include "pgdb2-file.h"
#include "pgdb2-struct.h"

namespace pagedb {

class DirEntry {
public:
	enum directory_ent_type d_type;
	std::string		key;
	std::string		key_end;
	std::string		value;
	uint32_t		ino_idx;

	uint32_t		key_len;
	uint32_t		key_end_len;
	uint32_t		value_len;

	DirEntry() : d_type(DE_NONE), ino_idx(0) {}

	void clear() {
		d_type = DE_NONE;
		key.clear();
		key_end.clear();
		value.clear();
		ino_idx = 0;
		key_len = 0;
		key_end_len = 0;
		value_len = 0;
	}
};

class Dir {
public:
	std::vector<DirEntry>	ents;

	void clear() {
		ents.clear();
	}
	void eraseIdx(size_t idx) { ents.erase(ents.begin() + idx); }
	void decode(const std::vector<unsigned char>& buf);
	void encode(std::vector<unsigned char>& buf) const;

	bool match(const std::string& key, unsigned int& idx) const;
};

class Inode {
public:
	bool		unused;			// unused slot in inode table

	uint64_t	e_ref;			// extent list page
	uint32_t	e_alloc;		// extent list alloc'd len
	std::vector<Extent> ext;		// extent list

	uint32_t size() const {
		uint32_t total = 0;
		for (std::vector<Extent>::const_iterator it = ext.begin();
		     it != ext.end(); it++)
			total += (*it).ext_len;

		return total;
	}

	void read(File& f, std::vector<unsigned char>& pagebuf) const;
	void write(File& f, const std::vector<unsigned char>& pagebuf) const;
};

class InodeTable {
public:
	std::vector<Inode> inodes;

	size_t size() const { return inodes.size(); }

	const Inode& getIdx(uint32_t idx) {
		if (idx >= inodes.size())
			throw std::runtime_error("InodeTable idx out of range");
		return inodes[idx];
	}

	void clear() { inodes.clear(); }
	void reserve(size_t n) { inodes.reserve(n); }
	void push_back(const Inode& ino) { inodes.push_back(ino); }

	void decode(std::vector<unsigned char>& buf);
	void encode(std::vector<unsigned char>& inotab_buf) const;
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
	InodeTable	inotab;

public:
	DB(std::string filename_, const Options& opt_);
	~DB();

private:
	void open();

	void readSuperblock();
	void readInodeTable();
	void readDir(uint32_t ino_idx, Dir& d);
	void readExtList(std::vector<Extent> &ext_list, uint64_t ref, uint32_t len = 1);

	void writeSuperblock();
	void writeInodeTable();
	void writeDir(uint32_t ino_idx, const Dir& d);
	void writeExtList(const std::vector<Extent>& ext_list, uint64_t ref, uint32_t max_len = 1);

	void clear();

};

} // namespace pagedb

#endif // __PGDB2_H__
