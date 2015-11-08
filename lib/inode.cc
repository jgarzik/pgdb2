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

	size_t ofs = 0;

	// read each extent into consolidated buffer pagebuf
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

	// iter thru pagebuf, writing at extent boundaries
	for (std::vector<Extent>::const_iterator it = ext.begin();
	     it != ext.end(); it++) {
		const Extent& e = (*it);
		size_t byte_ofs = ofs * pgsz;

		// TODO: use ptr-based .write(), eliminate temp buffer
		std::vector<unsigned char> tmpbuf(pagebuf.begin() + byte_ofs,
						  pagebuf.begin() + byte_ofs + (e.ext_len * pgsz));

		f.write(tmpbuf, e.ext_page, e.ext_len);

		ofs += e.ext_len;
	}
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

} // namespace pagedb

