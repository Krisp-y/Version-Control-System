### COMP1521 20T3: ass2: blobby ###

Task: write a file archiver (blobby.c).

The file archives in this assignment are called blobs.
Each blob contains one or more blobettes.
Each blobette records one file system object.
Their format is described below.

blobby.c should be able to:

list the contents of a blob (subset 0),
list the permissions of files in a blob (subset 0),
list the size (number of bytes) of files in a blob (subset 0),
check the blobette magic number (subset 0),
extract files from a blob (subset 1),
check blobette integrity (hashes) (subset 1),
set the file permissions of files extracted from a blob (subset 1),
create a blob from a list of files (subset 2),
list, extract, and create blobs that include directories (subset 3),
list, extract, and create blobs that are compressed (subset 4).

###Usage
##Subset 0
Given the -l command line argument blobby.c should for each file in the specified blob print:
The file/directory permissions in octal
The file/directory size in bytes
The file/directory pathname

##Subset 1
Given the -x command line argument blobby.c should:
Extract the files in the specified blob.

It should set file permissions for extracted files to the permissions specified in the blob.

It should check blob integrity by checking each blobette hash, and emit an error if any are incorrect.

##Subset 2
Given the -c command line argument blobby.c should:
Create a blob containing the specified files.

##Subset 3
Given the -c command line argument blobby.c should:
Be able to add files in sub-directories, for examples:

