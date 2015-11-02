#ifndef __PGDB2_STRUCT_H__
#define __PGDB2_STRUCT_H__
/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "pgdb2-config.h"

#include <stdint.h>
#include <string>
#include "endian_compat.h"

namespace pagedb {

enum inode_constants {
	DBINO_TABLE	= 0,			// inode table
	DBINO_FREELIST	= 1,			// list of free extents
	DBINO_ROOT_DIR	= 2,			// root directory

	DBINO__LAST = DBINO_ROOT_DIR
};

#define SB_MAGIC "PGDB0000"
#define INOTAB_MAGIC "PGIT0000"
#define INOTABENT_MAGIC "PGIE0000"
#define DIR_MAGIC "PGDR0000"
#define DIRENT_MAGIC "PGDE0000"

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

	void swap_n2h() {
		version = le32toh(version);
		page_size = le32toh(page_size);
		features = le64toh(features);
		inode_table_ref = le64toh(inode_table_ref);
	}
	void swap_h2n() {
		version = htole32(version);
		page_size = htole32(page_size);
		features = htole64(features);
		inode_table_ref = htole64(inode_table_ref);
	}
	bool valid() const {
		if ((version < 1) ||
		    (page_size < 512) || (page_size > 65536) ||
		    ((!(features & SBF_MBO)) || (features & SBF_MBZ)) ||
		    (inode_table_ref < 1))
			return false;

		if (std::string((const char *)magic, sizeof(magic)) != SB_MAGIC)
			return false;

		return true;
	}
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

	void swap_n2h() {
		ext_page = le64toh(ext_page);
		ext_len = le32toh(ext_len);
		ext_flags = le32toh(ext_flags);
	}
	void swap_h2n() {
		ext_page = htole64(ext_page);
		ext_len = htole32(ext_len);
		ext_flags = htole32(ext_flags);
	}
	bool valid() const {
		if ((ext_page == 0) ||
		    (ext_len == 0) ||
		    (!(ext_flags & EF_MBO)) ||
		    (ext_flags & EF_MBZ))
			return false;

		return true;
	}
	bool isNull() const {
		return (!ext_page && !ext_len && (ext_flags == EF_MBO));
	}
};

enum inodetable_flags {
	ITF_MBO		= (1U << 31),		// must be one
	ITF_MBZ		= (1U << 30),		// must be zero
	ITF_HDR		= (1U << 29),		// hdr rec
	ITF_EXT_INT	= (1U << 28),		// ext list in inode table
};

struct InodeTableHdr {
	unsigned char	magic[8];		// record unique id
	uint32_t	it_len;			// number of entries in table
	uint32_t	it_flags;		// flags bitmask

	void swap_n2h() {
		it_len = le32toh(it_len);
		it_flags = le32toh(it_flags);
	}
	void swap_h2n() {
		it_len = htole32(it_len);
		it_flags = htole32(it_flags);
	}
	bool valid() const {
		if ( /* (it_len == 0) || */
		    (!(it_flags & ITF_MBO)) ||
		    (it_flags & ITF_MBZ))
			return false;

		if (it_flags & ITF_HDR) {
			if (std::string((const char *)magic, sizeof(magic)) != INOTAB_MAGIC)
				return false;
		} else {
			if (std::string((const char *)magic, sizeof(magic)) != INOTABENT_MAGIC)
				return false;
		}

		return true;
	}
};

enum directory_ent_type {
	DE_NONE		= 0,
	DE_DIR		= 1,
	DE_KEY		= 2,
	DE_KEY_VALUE	= 3,

	DE__LAST	= DE_KEY
};

enum directory_flags {
	DF_MBO		= (1U << 31),		// must be one
	DF_MBZ		= (1U << 30),		// must be zero

	DF_ENT_TYPE	= 0xf,			// dirent type mask
};

struct DirectoryHdr {
	unsigned char	magic[8];		// record unique id
	uint32_t	d_len;			// number of entries in table
	uint32_t	d_flags;		// flags bitmask

	void swap_n2h() {
		d_len = le32toh(d_len);
		d_flags = le32toh(d_flags);
	}
	void swap_h2n() {
		d_len = htole32(d_len);
		d_flags = htole32(d_flags);
	}
	bool valid() const {
		if ((!(d_flags & DF_MBO)) ||
		    (d_flags & DF_MBZ))
			return false;

		if (std::string((const char *)magic, sizeof(magic)) != DIR_MAGIC)
			return false;

		return true;
	}
};

struct DirectoryEnt {
	unsigned char	magic[8];		// record unique id
	uint32_t	de_flags;		// flags bitmask
	uint32_t	de_key_len;		// key len
	uint32_t	de_val_len;		// key-end or value len
	uint32_t	de_ino;			// dir or value inode

	void swap_n2h() {
		de_flags = le32toh(de_flags);
		de_key_len = le32toh(de_key_len);
		de_val_len = le32toh(de_val_len);
		de_ino = le32toh(de_ino);
	}
	void swap_h2n() {
		de_flags = htole32(de_flags);
		de_key_len = htole32(de_key_len);
		de_val_len = htole32(de_val_len);
		de_ino = htole32(de_ino);
	}
	enum directory_ent_type dType() const {
		return (enum directory_ent_type) (de_flags & DF_ENT_TYPE);
	}
	bool valid() const {
		if ((!(de_flags & DF_MBO)) ||
		    (de_flags & DF_MBZ) ||
		    (dType() > DE__LAST))
			return false;

		if (std::string((const char *)magic, sizeof(magic)) != DIRENT_MAGIC)
			return false;

		return true;
	}
};

} // namespace pagedb

#endif // __PGDB2_STRUCT_H__
