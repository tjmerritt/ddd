
Disk to Disk copy with Defect recovery
======================================

I use this utility to recover data off of dying disk drives.  It will copy
all of the readable data from the source disk drive and write an output
image with the readable data plus any unreadable data replaced with zeros.

This allows you to read the dying disk drive in one pass, get as much data
as possible, get the locations of any unreadable data, and then use the 
copy of the disk drive to extract any files that are extractable.  Mounting
the drive as a file system may kill it with the disk head seek activity and 
potential redundant re-reading of data.

Usage
-----

The default block size used is 64K bytes as this is the largest size that an
ATA drive can transfer.  Some SATA drives support larger transfers so this
should be make adjustable in the future.  The -f option can be used to get
fine grain bad block information.  This will read at the 512 byte sector
level.  Modern disks have larger sector sizes so this isn't as useful as 
it has been in the past.  This should also be made adjustable since 4K
physical sectors are now common.

Finding bad blocks:

    # ddd /dev/da0

Finding bad blocks on a portion of the disk:

    # ddd /dev/da0 <staring block number> <number of blocks>

Finding fine grain bad block information:

    # ddd -f /dev/da0 <staring block number> <number of blocks>

Create an image of a dying disk drive:

    # ddd -o image.dmg /dev/da0 

History
-------
The original version of this program (you can find it in the history) was
the disk defect detector.  It would scan a disk looking for bad blocks
and their location.  I would then use the list of offsets and the dd
command to write zeros to the bad blocks in hopes of being able to 
repair the file system and recover files.  Later versons add the -f
option to automatically overwrite bad blocks with zeros.

The latest version  allows the disk to be copied to a file on another
disk drive replacing bad blocks with zeros.  The helpful feature is the
progress indicator which gives and indication of the readablity of various
portions of the disk.  If it is moving slowly the disk driver is likely
doing a lot of retries in order to get the data.  If you have a 250GB you
can have some idea where you are if you have progressed to 80GB.
