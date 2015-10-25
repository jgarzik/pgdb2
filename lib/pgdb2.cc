
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdexcept>
#include <pgdb2.h>

namespace pagedb {

DB::DB(std::string filename_, const Options& opt_)
{
	running = false;
	fd = -1;
	filename = filename_;
	options = opt_;

	open();
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

	if (options.f_create)
		flags |= O_CREAT;

	fd = ::open(filename.c_str(), flags, 0666);
	if (fd < 0)
		throw std::runtime_error("Failed open " + filename + ": " + strerror(errno));

	running = true;
}

DB::~DB()
{
	if (!running)
		return;
	
	close(fd);
	fd = -1;

	running = false;
}

} // namespace pagedb

