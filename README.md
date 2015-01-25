
Disk to Disk copy with Defect recovery
======================================

I use this utility to recover data off of dying disk drives.  It will copy
all of the readable data from the source disk drive and write an output
image with the readable data plus any unreadable data replaced with zeros.

This allows you to read the dying disk drive in one pass, get as much data
as possible, get the locations of any unreadable data, and then use the 
copy of the disk driver to extract any files that are extractable.  

Usage
-----

Finding bad blocks:

    # ddd /dev/da0

finding bad blocks on a portion of the disk:

    # ddd /dev/da0 <staring block number> <number of blocks>

Overwriting bad blocks with zeros:

    # ddd -f /dev/da0 <staring block number> <number of blocks>

History
-------
The original version of this program (you can find it in the history) was
the disk defect detector.  It would scan a disk looking for bad blocks
and their location.  I would then use the list of offsets and the dd
command to write zeros to the bad blocks in hopes of being able to 
repair the file system and recover files.  Later versons add the -f
option to automatically overwrite bad blocks with zeros.

The latest version (not yet located and checked in) allows the disk to 
be copied to a file on another disk drive replacing bad blocks with 
zeros.  The helpful feature is the progress indicator which gives and
indication of the readablity of various portions of the disk.  If it
is moving slowly the disk driver is likely doing a lot of retries in
order to get the data.  If you have a 250GB you can have some idea
where you are if you have progressed to 80GB.
