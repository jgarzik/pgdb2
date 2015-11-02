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

namespace pagedb {

void Inode::read(File& f, std::vector<unsigned char>& pagebuf) const
{
	size_t pgsz = f.pageSize();
	uint32_t n_pages = size();

	assert(pagebuf.size() >= (pgsz * n_pages));

	size_t ofs = 0;

	for (std::vector<Extent>::const_iterator it = ext.begin();
	     it != ext.end(); it++) {
		const Extent& e = (*it);
		std::vector<unsigned char> tmpbuf(pgsz * e.ext_len);

		f.read(tmpbuf, e.ext_page, e.ext_len);

		assert(pagebuf.size() >= ((ofs * pgsz) + tmpbuf.size()));
		memcpy(&pagebuf[ofs * pgsz], &tmpbuf[0], tmpbuf.size());

		ofs += e.ext_len;
	}
}

void Inode::write(File& f, const std::vector<unsigned char>& pagebuf) const
{
	size_t pgsz = f.pageSize();
	uint32_t n_pages = size();

	assert(pagebuf.size() <= (pgsz * n_pages));

	size_t ofs = 0;

	for (std::vector<Extent>::const_iterator it = ext.begin();
	     it != ext.end(); it++) {
		const Extent& e = (*it);
		size_t byte_ofs = ofs * pgsz;
		std::vector<unsigned char> tmpbuf(pagebuf.begin() + byte_ofs,
						  pagebuf.begin() + byte_ofs + (e.ext_len * pgsz));

		f.write(tmpbuf, e.ext_page, e.ext_len);

		ofs += e.ext_len;
	}
}

void Dir::decode(const std::vector<unsigned char>& buf)
{
	// clear self
	clear();

	// initialize buffer walk
	const unsigned char *p = &buf[0];
	uint32_t bytes = buf.size();

	// Decode directory header
	if (bytes < sizeof(DirectoryHdr))
		throw std::runtime_error("Dir hdr short read");

	DirectoryHdr* hdr = (DirectoryHdr *) p;
	p += sizeof(DirectoryHdr);
	bytes -= sizeof(DirectoryHdr);

	hdr->swap_n2h();
	if (!hdr->valid())
		throw std::runtime_error("Dir hdr corrupted");

	// quick size sanity check and pre-alloc
	if (bytes < (sizeof(DirectoryEnt) * hdr->d_len))	// rough
		throw std::runtime_error("Dir truncated");

	ents.reserve(hdr->d_len);

	// Decode directory entries
	for (unsigned int dir_idx = 0; dir_idx < hdr->d_len; dir_idx++) {

		// Decode fixed length portion of dir ent
		if (bytes < sizeof(DirectoryEnt))
			throw std::runtime_error("Dir ent truncated");

		DirectoryEnt* buf_de = (DirectoryEnt *) p;
		p += sizeof(DirectoryEnt);
		bytes -= sizeof(DirectoryEnt);

		buf_de->swap_n2h();
		if (!buf_de->valid())
			throw std::runtime_error("Dir ent buf corrupted");

		DirEntry de;
		de.d_type = (enum directory_ent_type)(buf_de->de_flags & DF_ENT_TYPE);
		de.ino_idx = buf_de->de_ino;

		// Decode variable length trailer
		switch (de.d_type) {
		case DE_NONE:
		default:
			throw std::runtime_error("Invalid dirent type");

		case DE_DIR:
			if (bytes < buf_de->de_key_len)
				throw std::runtime_error("Invalid dirent ksz");
			de.key.assign((const char *) p, buf_de->de_key_len);
			p += buf_de->de_key_len;
			bytes -= buf_de->de_key_len;

			if (bytes < buf_de->de_val_len)
				throw std::runtime_error("Invalid dirent kesz");
			de.key_end.assign((const char *) p, buf_de->de_val_len);
			p += buf_de->de_val_len;
			bytes -= buf_de->de_val_len;
			break;

		case DE_KEY:
			if (bytes < buf_de->de_key_len)
				throw std::runtime_error("Invalid dirent ksz");
			de.key.assign((const char *) p, buf_de->de_key_len);
			p += buf_de->de_key_len;
			bytes -= buf_de->de_key_len;
			break;

		case DE_KEY_VALUE:
			if (bytes < buf_de->de_key_len)
				throw std::runtime_error("Invalid dirent ksz");
			de.key.assign((const char *) p, buf_de->de_key_len);
			p += buf_de->de_key_len;
			bytes -= buf_de->de_key_len;

			if (bytes < buf_de->de_val_len)
				throw std::runtime_error("Invalid dirent vsz");
			de.value.assign((const char *) p, buf_de->de_val_len);
			p += buf_de->de_val_len;
			bytes -= buf_de->de_val_len;
			break;
		}

		ents.push_back(de);
	}
}

void Dir::encode(std::vector<unsigned char>& buf) const
{
	// pre allocate buffer
	buf.clear();
	buf.reserve(sizeof(DirectoryHdr) +
		    (sizeof(DirectoryEnt) * ents.size()));

	// Encode directory header
	DirectoryHdr hdr;
	memcpy(hdr.magic, DIR_MAGIC, sizeof(hdr.magic));
	hdr.d_len = ents.size();
	hdr.d_flags = DF_MBO;
	hdr.swap_h2n();

	unsigned char *p = (unsigned char *) &hdr;
	buf.insert(buf.end(), p, p + sizeof(hdr));

	// Encode directory entries
	for (std::vector<DirEntry>::const_iterator it = ents.begin();
	     it != ents.end(); it++) {
		const DirEntry& de = (*it);

		// Encode fixed length directory entry
		DirectoryEnt buf_de;
		memcpy(buf_de.magic, DIRENT_MAGIC, sizeof(buf_de.magic));
		buf_de.de_flags = DF_MBO | de.d_type;

		switch (de.d_type) {
		case DE_NONE:
		default:
			// should never happen
			assert(0);
			break;

		case DE_DIR:
			buf_de.de_key_len = de.key.size();
			buf_de.de_val_len = de.key_end.size();
			buf_de.de_ino = de.ino_idx;
			break;

		case DE_KEY:
			buf_de.de_key_len = de.key.size();
			buf_de.de_val_len = 0;
			buf_de.de_ino = de.ino_idx;
			break;

		case DE_KEY_VALUE:
			buf_de.de_key_len = de.key.size();
			buf_de.de_val_len = de.value.size();
			buf_de.de_ino = 0;
			break;
		}

		buf_de.swap_h2n();

		unsigned char *p = (unsigned char *) &buf_de;
		buf.insert(buf.end(), p, p + sizeof(buf_de));

		// Encode variable length dirent trailer
		switch (de.d_type) {
		case DE_NONE:
		default:
			// should never happen
			assert(0);
			break;

		case DE_DIR:
			buf.insert(buf.end(), de.key.begin(), de.key.end());
			buf.insert(buf.end(), de.key_end.begin(), de.key_end.end());
			break;

		case DE_KEY:
			buf.insert(buf.end(), de.key.begin(), de.key.end());
			break;

		case DE_KEY_VALUE:
			buf.insert(buf.end(), de.key.begin(), de.key.end());
			buf.insert(buf.end(), de.value.begin(), de.value.end());
			break;
		}

	}
}

DB::DB(std::string filename_, const Options& opt_)
{
	running = false;

	filename = filename_;
	options = opt_;

	open();
	readSuperblock();
	readInodeTable();
	readDir(DBINO_ROOT_DIR, rootDir);

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
	writeDir(DBINO_ROOT_DIR, rootDir);

	f.sync();
}

void DB::readSuperblock()
{
	assert(f.isOpen());

	if ((f.size() == 0) && (options.f_create)) {
		clear();
		return;
	}

	std::vector<unsigned char> sb_buf(4096);
	f.read(sb_buf, 0);

	memcpy(&sb, &sb_buf[0], sizeof(sb));

	sb.swap_n2h();

	if (!sb.valid())
		throw std::runtime_error("Superblock invalid");

	f.setPageSize(sb.page_size);
}

void DB::writeSuperblock()
{
	assert(f.isOpen());

	Superblock write_sb(sb);
	write_sb.swap_h2n();

	std::vector<unsigned char> page(sb.page_size);
	memcpy(&page[0], &write_sb, sizeof(write_sb));

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

	// initialize buffer walk
	unsigned char *p = &inotab_buf[0];
	uint32_t bytes = inotab_buf.size();

	// inode table header
	if (bytes < sizeof(InodeTableHdr))
		throw std::runtime_error("inode table hdr short read");

	InodeTableHdr* ith = (InodeTableHdr *) p;
	p += sizeof(InodeTableHdr);
	bytes -= sizeof(InodeTableHdr);

	ith->swap_n2h();

	if (!ith->valid())
		throw std::runtime_error("Inode table invalid header");
	if (inotab_buf.size() < (ith->it_len * (sizeof(InodeTableHdr)+sizeof(Extent)))) //rough
		throw std::runtime_error("Inode table invalid length");

	// walk inode table entries
	for (unsigned int idx = 0; (bytes > 0) && (idx < ith->it_len); idx++) {

		// Decode inode table entry header
		if (bytes < sizeof(InodeTableHdr))
			throw std::runtime_error("inode table ent short read");

		InodeTableHdr* hdr = (InodeTableHdr *) p;
		p += sizeof(InodeTableHdr);
		bytes -= sizeof(InodeTableHdr);

		hdr->swap_n2h();
		if (!hdr->valid())
			throw std::runtime_error("Inode table ent invalid");

		// Decode inode table extent data
		if (bytes < sizeof(Extent))
			throw std::runtime_error("inode table ent ext short read");

		Extent* e = (Extent *) p;
		p += sizeof(Extent);
		bytes -= sizeof(Extent);

		e->swap_n2h();

		bool ext_empty = false;
		if (e->isNull())
			ext_empty = true;
		else if (!e->valid())
			throw std::runtime_error("Inode table ext invalid");

		Inode ino;

		// empty (null) extent
		if (ext_empty) {
			ino.e_ref = 0;
			ino.e_alloc = 0;

		// internal extent
		} else if (hdr->it_flags & ITF_EXT_INT) {
			ino.e_ref = 0;
			ino.e_alloc = 0;
			ino.ext.push_back(*e);

		// external extent, read from storage
		} else {
			ino.e_ref = e->ext_page;
			ino.e_alloc = e->ext_len;
			readExtList(ino.ext, ino.e_ref, ino.e_alloc);
		}

		// add to inode table in memory
		inotab.push_back(ino);
	}

	if (inotab.size() < (DBINO__LAST+1))
		throw std::runtime_error("Inode table truncated");
}

void InodeTable::encode(std::vector<unsigned char>& inotab_buf)
{
	assert(inodes.size() > 0);

	inotab_buf.clear();
	inotab_buf.reserve(inodes.size() * (sizeof(InodeTableHdr)+sizeof(Extent)));

	InodeTableHdr ith;
	memcpy(ith.magic, INOTAB_MAGIC, sizeof(ith.magic));
	ith.it_len = inodes.size() - 1;
	ith.it_flags = ITF_MBO | ITF_HDR;
	ith.swap_h2n();

	unsigned char *p = (unsigned char *) &ith;
	inotab_buf.insert(inotab_buf.end(), p, p + sizeof(ith));

	for (unsigned int idx = 0; idx < inodes.size(); idx++) {
		// skip; special case: ext list written manually
		if (idx == DBINO_TABLE)
			continue;

		const Inode& ino = inodes[idx];
		bool int_list = (ino.e_ref == 0);
		assert((!int_list) || (int_list && (ino.ext.size() <= 1)));

		// encode inode table header
		InodeTableHdr hdr;
		memcpy(hdr.magic, INOTABENT_MAGIC, sizeof(hdr.magic));
		hdr.it_len = 0;
		hdr.it_flags = ITF_MBO | (int_list ? ITF_EXT_INT : 0);
		hdr.swap_h2n();

		p = (unsigned char *) &hdr;
		inotab_buf.insert(inotab_buf.end(), p, p + sizeof(hdr));

		Extent e;
		if (int_list) {
			if (ino.ext.size() > 0)
				e = ino.ext[0];
			else {
				e.ext_page = 0;
				e.ext_len = 0;
				e.ext_flags = EF_MBO;
			}
		} else {
			e.ext_page = ino.e_ref;
			e.ext_len = ino.e_alloc;
			e.ext_flags = EF_MBO;
		}
		e.swap_h2n();

		p = (unsigned char *) &e;
		inotab_buf.insert(inotab_buf.end(), p, p + sizeof(e));
	}
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

	f.write(pages, ref, max_len);
}

DB::~DB()
{
	if (!running)
		return;

	running = false;
}

} // namespace pagedb

