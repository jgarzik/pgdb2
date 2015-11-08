/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "pgdb2-config.h"

#include <stdint.h>
#include <vector>
#include <string.h>
#include <pgdb2.h>
#include <cassert>

namespace pagedb {

void Inode::read(File& f, std::vector<unsigned char>& pagebuf) const
{
	size_t pgsz = f.pageSize();
	uint32_t n_pages = size();

	if (pagebuf.size() < (pgsz * n_pages))
		pagebuf.resize(pgsz * n_pages);

	unsigned char *p = &pagebuf[0];

	// read each extent into consolidated buffer pagebuf
	for (std::vector<Extent>::const_iterator it = ext.begin();
	     it != ext.end(); it++) {
		const Extent& e = (*it);

		f.read((void *) p, e.ext_page, e.ext_len);

		p += (e.ext_len * pgsz);

		assert(p <= (&pagebuf[0] + pagebuf.size()));
	}
}

void Inode::write(File& f, const std::vector<unsigned char>& pagebuf) const
{
	size_t pgsz = f.pageSize();
	uint32_t n_pages = size();

	assert((pagebuf.size() % pgsz) == 0);
	assert(pagebuf.size() <= (pgsz * n_pages));

	const unsigned char *p = &pagebuf[0];
	uint32_t out_pages = pagebuf.size() / pgsz;

	// iter thru pagebuf, writing at extent boundaries
	for (std::vector<Extent>::const_iterator it = ext.begin();
	     (it != ext.end()) && (out_pages > 0); it++) {
		const Extent& e = (*it);

		size_t write_pages = e.ext_len;
		if (write_pages > out_pages)
			write_pages = out_pages;

		f.write((const void *)p, e.ext_page, write_pages);

		out_pages -= write_pages;
		p += (write_pages * pgsz);

		assert(p <= (&pagebuf[0] + pagebuf.size()));
	}
}

void InodeTable::decode(std::vector<unsigned char>& inotab_buf)
{
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

		// external extent
		} else {
			ino.e_ref = e->ext_page;
			ino.e_alloc = e->ext_len;

			// deferred: read ext list from storage
		}

		if (hdr->it_flags & ITF_UNUSED)
			ino.unused = true;

		// add to inode table in memory
		inodes.push_back(ino);
	}
}

void InodeTable::encode(std::vector<unsigned char>& inotab_buf) const
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

} // namespace pagedb

