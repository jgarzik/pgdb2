#!/bin/sh

./basic
retval=$?

if [ ! -f foo.db ]
then
	retval=1
fi

rm -f foo.db

exit $retval
