/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "pgdb2-config.h"

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

namespace page {

DB::DB(std::string filename_, const Options& opt_)
{
	running = false;

	filename = filename_;
	options = opt_;

	open();

	// special case: if create option + empty file
	// initialize brand new database structures
	if ((f.size() == 0) && (options.f_create)) {
		clear();
	} else {
		readSuperblock();
		readInodeTable();
	}

	// for verification
	Dir dummyDir;
	readDir(DBINO_ROOT_DIR, dummyDir);

	running = true;
}

void DB::open()
{
	int flags = 0;

	// build OS read/write options
	if (options.f_read && options.f_write)
		flags |= O_RDWR;
	else if (options.f_read)
		flags |= O_RDONLY;
	else
		throw std::runtime_error("Invalid read/write options");

	// build OS create option
	if (options.f_create) {
		if (!options.f_write)
			throw std::runtime_error("Invalid creat/write options");
		flags |= O_CREAT;
	}

	// open OS file
	f.open(filename, flags, sizeof(Superblock));
}

void DB::clear()
{
	memset(&sb, 0, sizeof(sb));
	inotab.clear();

	// init superblock
	memcpy(sb.magic, SB_MAGIC, sizeof(sb.magic));
	sb.version = 1;
	sb.page_size = 4096;
	sb.features = SBF_MBO;
	sb.inode_table_ref = 1;

	f.setPageSize(sb.page_size);

	// init inode table
	inotab.clear();
	inotab.reserve(16);

	// DBINO_TABLE(0): inode table
	Inode tab_ino;
	tab_ino.e_ref = 1;		// inode table elist start
	tab_ino.e_alloc = 1;		// inode table elist len

	Extent inoref;
	inoref.ext_page = 2;		// inode table offset=2
	inoref.ext_len = 1;		// inode table length=1
	inoref.ext_flags = EF_MBO;
	tab_ino.ext.push_back(inoref);

	inotab.push_back(tab_ino);
	assert(inotab.getIdx(DBINO_TABLE).size() == 1);

	// DBINO_FREELIST(1): list of free extents (empty)
	Inode freelist_ino;
	freelist_ino.e_ref = 0;
	freelist_ino.e_alloc = 0;
	freelist_ino.ext.clear();

	inotab.push_back(freelist_ino);
	assert(inotab.getIdx(DBINO_FREELIST).size() == 0);

	// DBINO_ROOT_DIR(2): root directory
	Inode root_ino;
	root_ino.e_ref = 0;
	root_ino.e_alloc = 0;

	Extent root_ref;
	root_ref.ext_page = 3;
	root_ref.ext_len = 1;
	root_ref.ext_flags = EF_MBO;
	root_ino.ext.push_back(root_ref);

	inotab.push_back(root_ino);

	// write everything
	writeSuperblock();
	writeInodeTable();
	// writeFreeList(); -- none to write

	Dir emptyDir;
	writeDir(DBINO_ROOT_DIR, emptyDir);

	f.sync();
}

void DB::readSuperblock()
{
	// read superblock into buffer
	std::vector<unsigned char> sb_buf(4096);
	f.read(sb_buf, 0);

	// fill and validate superblock struct
	memcpy(&sb, &sb_buf[0], sizeof(sb));

	sb.swap_n2h();

	if (!sb.valid())
		throw std::runtime_error("Superblock invalid");

	// reset page file size, now that it is known
	f.setPageSize(sb.page_size);
}

void DB::writeSuperblock()
{
	assert(f.isOpen());

	// copy current sb
	Superblock write_sb(sb);
	write_sb.swap_h2n();

	// copy sb to page
	std::vector<unsigned char> page(sb.page_size);
	memcpy(&page[0], &write_sb, sizeof(write_sb));

	// write to storage
	f.write(page, 0);
}

void DB::readInodeTable()
{
	inotab.clear();
	inotab.reserve(16);

	// magic inode #0 is the inode table itself; handle its
	// extent list as a special case
	Inode tab_ino;
	tab_ino.e_ref = sb.inode_table_ref;
	tab_ino.e_alloc = 1;
	readExtList(tab_ino.ext, tab_ino.e_ref);
	inotab.push_back(tab_ino);

	// read inode table buffer from storage
	uint32_t inotab_pages = tab_ino.size();
	std::vector<unsigned char> inotab_buf(inotab_pages * sb.page_size);
	tab_ino.read(f, inotab_buf);

	// decode buffer into inode table
	inotab.decode(inotab_buf);

	// deferred I/O from inotab decode step: read inode extent lists
	for (std::vector<Inode>::iterator it = inotab.inodes.begin();
	     it != inotab.inodes.end(); it++) {
		Inode& ino = (*it);
		if (ino.e_ref && ino.e_alloc)
			readExtList(ino.ext, ino.e_ref, ino.e_alloc);
	}

	if (inotab.size() < (DBINO__LAST+1))
		throw std::runtime_error("Inode table truncated");
}

void DB::writeInodeTable()
{
	std::vector<unsigned char> inotab_buf;
	inotab.encode(inotab_buf);

	// special case: inode table's own extent list
	const Inode& tab_ino = inotab.getIdx(DBINO_TABLE);
	assert(tab_ino.e_ref == sb.inode_table_ref);
	assert(tab_ino.e_alloc == 1);
	assert((tab_ino.size() * sb.page_size) >= inotab_buf.size());

	writeExtList(tab_ino.ext, tab_ino.e_ref);

	// inode table encoded data
	bufSizeAlign(inotab_buf, sb.page_size);
	tab_ino.write(f, inotab_buf);
}

void DB::readDir(uint32_t ino_idx, Dir& d)
{
	d.clear();

	// lookup inode
	assert(ino_idx < inotab.size());
	const Inode& dir_ino = inotab.getIdx(ino_idx);
	uint32_t n_pages = dir_ino.size();

	// read from storage into buffer
	std::vector<unsigned char> buf(n_pages * sb.page_size);
	dir_ino.read(f, buf);

	// decode directory buffer
	d.decode(buf);
}

void DB::writeDir(uint32_t ino_idx, const Dir& d)
{
	std::vector<unsigned char> buf;
	d.encode(buf);

	// lookup inode
	assert(ino_idx < inotab.size());
	const Inode& dir_ino = inotab.getIdx(ino_idx);
	assert((dir_ino.size() * sb.page_size) >= buf.size());

	// write root directory to storage
	bufSizeAlign(buf, sb.page_size);
	dir_ino.write(f, buf);
}

void DB::readExtList(std::vector<Extent> &ext_list, uint64_t ref, uint32_t len)
{
	ext_list.clear();

	// input page
	std::vector<unsigned char> page(sb.page_size);
	f.read(page, ref, len);

	// decode header
	const Extent *in_ext = (Extent *) &page[0];

	Extent hdr(in_ext[0]);
	hdr.swap_n2h();

	// check header
	if (hdr.ext_page != 0)
		throw std::runtime_error("Extent list invalid hdr page");
	if ((!(hdr.ext_flags & EF_MBO)) || (hdr.ext_flags & EF_MBZ) ||
	    (!(hdr.ext_flags & EF_HDR)))
		throw std::runtime_error("Extent list invalid hdr flags");

	// pre-size input
	ext_list.reserve(hdr.ext_len - 1);

	// decode extent list
	for (unsigned int i = 1; i < hdr.ext_len; i++) {
		Extent e(in_ext[i]);
		e.swap_n2h();

		if (e.ext_page == 0)
			throw std::runtime_error("Extent list invalid page");
		if ((!(e.ext_flags & EF_MBO)) || (e.ext_flags & EF_MBZ) ||
		    (e.ext_flags & EF_HDR))
			throw std::runtime_error("Extent list invalid flags");

		ext_list.push_back(e);
	}
}

void DB::writeExtList(const std::vector<Extent>& ext_list,
		      uint64_t ref, uint32_t max_len)
{
	assert(((ext_list.size() + 1) * sizeof(Extent)) <= (max_len * sb.page_size));

	std::vector<unsigned char> pages(sb.page_size * max_len);

	Extent *out_ext = (Extent *) &pages[0];

	// encode header
	out_ext[0].ext_page = 0;
	out_ext[0].ext_len = ext_list.size() + 1;
	out_ext[0].ext_flags = EF_MBO | EF_HDR;
	out_ext[0].swap_h2n();

	// encode list
	unsigned int out_idx = 1;
	for (unsigned int i = 0; i < ext_list.size(); i++) {
		const Extent& in_ext = ext_list[i];

		if (((out_idx+1) * sizeof(Extent)) > pages.size())
			throw std::runtime_error("Extent list exceeds max");

		out_ext[out_idx] = in_ext;
		out_ext[out_idx].swap_h2n();

		out_idx++;
	}

	// write to storage
	f.write(pages, ref, max_len);
}

DB::~DB()
{
	if (!running)
		return;

	running = false;
}

} // namespace page

