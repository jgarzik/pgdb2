
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

DB::DB(std::string filename_, const Options& opt_)
{
	running = false;

	filename = filename_;
	options = opt_;

	open();
	readSuperblock();
	readInodeTable();

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
	inodes.clear();

	// init superblock
	memcpy(sb.magic, SB_MAGIC, sizeof(sb.magic));
	sb.version = 1;
	sb.page_size = 4096;
	sb.features = SBF_MBO;
	sb.inode_table_ref = 1;

	f.setPageSize(sb.page_size);

	// init inode table
	inodes.resize(1);

	Extent inoref;
	inoref.ext_page = 2;			// inode table offset=2
	inoref.ext_len = 1;			// inode table length=1
	inoref.ext_flags = EF_MBO;

	inodes[DBINO_TABLE].e_ref = 1;		// inode table elist start
	inodes[DBINO_TABLE].e_alloc = 1;	// inode table elist len
	inodes[DBINO_TABLE].ext.push_back(inoref);

	assert(inodes[DBINO_TABLE].size() == 1);

	// write everything
	writeSuperblock();
	writeInodeExtList(DBINO_TABLE);
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

	Superblock write_sb;
	memcpy(&write_sb, &sb, sizeof(sb));

	write_sb.swap_h2n();

	std::vector<unsigned char> page;
	page.resize(sb.page_size);
	memcpy(&page[0], &write_sb, sizeof(write_sb));

	f.write(page, 0);
}

void DB::readInodeTable()
{
	inodes.clear();
	inodes.resize(1);

	// magic inode #0 is the inode table itself; handle its
	// extent list as a special case
	inodes[DBINO_TABLE].e_ref = sb.inode_table_ref;
	inodes[DBINO_TABLE].e_alloc = 1;
	readExtList(inodes[DBINO_TABLE].ext, inodes[DBINO_TABLE].e_ref);
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

	std::vector<unsigned char> pages;
	pages.resize(sb.page_size * max_len);

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

		memcpy(&out_ext[out_idx], &in_ext, sizeof(in_ext));
		out_ext[out_idx].swap_h2n();

		out_idx++;
	}

	f.write(pages, ref, max_len);
}

void DB::writeInodeExtList(uint32_t ino)
{
	const std::vector<Extent>& ext_list = inodes[ino].ext;
	writeExtList(ext_list, inodes[ino].e_ref, inodes[ino].e_alloc);
}

DB::~DB()
{
	if (!running)
		return;

	running = false;
}

} // namespace pagedb

