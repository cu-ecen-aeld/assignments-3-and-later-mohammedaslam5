#!/bin/sh

filesdir=$1
searchstr=$2

if [ ! $# -eq 2 ]
then
	if [ -z "$filesdir" ]
	then
		echo "filesdir parameter not specified"
	fi
	if [ -z "$searchstr" ]
        then
                echo "searchstr parameter not specified"
        fi
	exit 1
fi

if [ ! -d "$filesdir" ]
then
	echo "$filesdir does not represent a directory on the filesystem"
	exit 1
fi

num_of_files=$(find "$filesdir"/ -type f -name "*" | wc -l)
num_of_matches=$(grep -r "$searchstr" "$filesdir"/ | wc -l)

echo "The number of files are $num_of_files and the number of matching lines are $num_of_matches"

