/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "pgdb2-config.h"

#include <stdint.h>
#include <cassert>
#include <vector>
#include <string.h>
#include <pgdb2.h>

namespace pagedb {

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

			de.key_len = buf_de->de_key_len;
			de.key_end_len = buf_de->de_val_len;
			break;

		case DE_KEY:
			if (bytes < buf_de->de_key_len)
				throw std::runtime_error("Invalid dirent ksz");
			de.key.assign((const char *) p, buf_de->de_key_len);
			p += buf_de->de_key_len;
			bytes -= buf_de->de_key_len;

			de.key_len = buf_de->de_key_len;
			de.value_len = buf_de->de_val_len;
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

			de.key_len = buf_de->de_key_len;
			de.value_len = buf_de->de_val_len;
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

bool Dir::match(const std::string& key, unsigned int& idx) const
{
	for (idx = 0; idx < ents.size(); idx++) {
		const DirEntry& ent = ents[idx];

		int key_cmp = key.compare(ent.key);
		if (key_cmp < 0)
			return false;
		if (key_cmp == 0)
			return true;

		if ((ent.d_type == DE_DIR) &&
		    (key.compare(ent.key_end) <= 0))
			return true;
	}

	return false;
}

} // namespace pagedb

