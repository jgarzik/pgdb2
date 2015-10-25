#ifndef __PGDB2_H__
#define __PGDB2_H__

#include <string>

namespace pagedb {

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

public:
	DB(std::string filename_, const Options& opt_);
	~DB();

private:
	void open();

};

} // namespace pagedb

#endif // __PGDB2_H__
