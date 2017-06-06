# os-team20

## Project 4, Finally!
We may go home. We do not need to weep all night!!!! This all-night work was the last pernoctation.

## High-Level Design
We followed T.A's high level guideline in this project.

## Implementation
### inode / inode_info
Add new informations to existing inode structures, to follow conventions.

```
i_lat_integer (32-bits)
i_lat_fractional (32-bits)
i_lng_integer (32-bits)
i_lng_fractional (32-bits)
i_accuracy (32-bits)
```
In ext2_inode, these are `le32` type (little endian 32) and in ext2_inode_info, these are `u32` type.


* Current GPS location is stored in global variable, and is protected by spinlock for SMP environment.

###  ext2_set/get_location
ext2_set_gps_location is set inode to current gps. It is called when i_mtime (modification time is changed), and in generic_mmap.
We add this as follows.
```
ialloc.c : struct inode *ext2_new_inode(struct inode *dir, umode_t mode, const struct qstr *qstr)
inode.c  : static int ext2_setsize(struct inode *inode, loff_t newsize)
super.c  : static ssize_t ext2_quota_write(struct super_block *sb, int type,const char *data, size_t len, loff_t off)
namei.c  : static int ext2_create (struct inode * dir, struct dentry * dentry, umode_t mode, bool excl)
```

Also, we modify `inode.c  : static int __ext2_write_inode(struct inode *inode, int do_sync)`

ext2_get_gps_location is only using in get_gps_location system call. It set gps to inode's gps information.

### Permission check.
We made a function `ext2_permission()` which checks the geographical accessibility(See Permission Policy) and general permission.

### Supporting mmap.
Users can use mmap to modify the file. We supported this case by modifying  `int generic_file_mmap(struct file * file, struct vm_area_struct * vma)` in mm/filemap.c

## System Call
We implemented two system calls: one(set_gps_location) is for setting current gps position(which is stored in kernel), and the other(get_gps_location) is for getting gps location of a specific file.

## User program
### gpsupdate
gps_update.c runs with three parameters: `latitude`, `longitude`, and `accuracy`. The first two arguments should be real numbers, whose significant figures should be less or equal to 6. The last argument should be an integer.

### file_loc
Using get_gps_location system call, read the path and return google maps link about file's coordinates. 
the format of link is `https://www.google.co.kr/maps/place/XXXXXX.XXXXXX°N+XXXXXX.XXXXXX°E`

## Tests
### e2fsprogs
We add some variables about gps to `ext2_inode` and `ext2_inode_large` in `ext2_fs.h`. The varible's order are same as fs's.

### mmap test
When a file is modified, it's gps location values should be updated with current ones. This is also true for file modification using memory mapped I/O. Our test program mmap_test.c simply reads a file, and then modify file using mmap function. When we ran this program on a specific file, we could confirm that it's location is updated as well.

### file_loc test
file_loc.c prints out the location of specific file in a google map link format. We tested this program for several files and checked if there links are correct.

### permission test
A file written in a location should not be opened in other locations that are far away. We made a test script `access_location_test.sh` which modifies location and try to read/write some files. When it tried to access files that it doesn't have permission, we could see that permission denied error occurs.

## Permission Policy
Since linux doesn't support floating point operations inside kernel, we used following formulas to compare file location with current location(GPS hardware location).

```
Suppose x is physical distance(in km).
With R = 6400km(earth radius) and theta(angle in degrees),
x = R * theta*(360/2pi)
  
When theta is 0.000001 (in other words, fractional part of degree equals to 1)
x = 6400 * 0.000001 * 360 / 2pi
  = 0.111701m
  
This means that small change in integer part of degree becomes significantly large when converted to distance.
When two integer part have diffrent values, we can assume that two locations are far enough from each other.
Thus, our kernel
1. Checks if integer parts of latitude and longitude are equal.
2. If equal, checks if the distance calculated from fractional parts, is less than sum of two accuracy values.

Supposing a and b represent two locations, fractional part compare logic is described as below:
((a_lat_fractional_part - b_lat_fractional_part) / 9)^2 + ((a_lng_fractional_part - b_lng_fractional_part) / 9)^2
      < (accuracy_a + accuracy_b)^2
```

## Lessons Learned
* Ext2 File System was simpler than we thought. But the project was much harder than we thought.
* Now, we admire the kernel programmer. They seem to be GOD
* At the end of hardship comes hapiness.
* At the end of hapiness comes another project.
* At the end of last project comes JongGang(Probably).

## P.S.
This time, we wrote README quite shortly to lessen the burden of our great T.A.s
Thank you so much.
