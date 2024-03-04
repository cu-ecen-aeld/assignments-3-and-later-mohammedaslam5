#!/bin/sh

writefile=$1
writestr=$2

if [ ! $# -eq 2 ]
then
	if [ -z $writefile ]
	then
		echo "writefile parameter not specified"
	fi
	if [ -z $writestr ]
        then
                echo "writestr parameter not specified"
        fi
	exit 1
fi

if [ ! -e $writefile ]
then
	DIR=${writefile%/*}
	mkdir -p $DIR
else
	echo "file exist"
fi

echo "$writestr" > $writefile

if [ ! -e $writefile ]
then
	echo "$writefile file could not be created"
	exit 1
fi

