#!/bin/sh

TESTFILES=file.db

./file
retval=$?

rm -f $TESTFILES

exit $retval
