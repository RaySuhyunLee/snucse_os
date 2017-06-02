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

### Pzzzermission check.
make `ext2_permission` function which check the geographical accessibility(See Permission Policy) and general permission.

### Supporting mmap.
 User can use mmap for modifying the file. We supported this situation by modifying  `int generic_file_mmap(struct file * file, struct vm_area_struct * vma)` in mm/filemap.c

## System Call
//TODO Suhyun

## User program
### gpsupdate
//TODO Suhyun

### file_loc
Using get_gps_location system call, read the path and return google maps link about file's coordinates. 
the form of link is `https://www.google.co.kr/maps/place/XXXXXX.XXXXXX°N+XXXXXX.XXXXXX°E`

## Test
###
//TODO Suhyun
### mmap test
read file on C program, modify file using mmap function.


## Permission Policy
Since linux doesn't support floating point operations inside kernel, we used following formulas to compare file location with current location(GPS hardware location).

```
Suppose x is physical distance(in km).
With R = 6400km(earth radius) and theta(angle in degrees),
x = R * theta*(360/2pi)
  
When theta is 0.000001 (in other words, fractional part of degree equals to 1)
x = 6400 * 0.000001 * 360 / 2pi
  = 0.111701m
  
This means that small change in integer part of degree becomes significantly large when converted to distance. In this case we can assume that two locations are far enough from each other.
Thus, our kernel
1. Checks if integer parts of latitude and longitude are equal.
2. If equal, checks if distance calculated from fractional parts, is less than sum of two accuracy values.
```

## Lessons Learned
* Ext2 File System is simpler than what we thought. But the project is much harder than our thought.
* Now, we admire the kernel programmer. They seem to be GOD
* At the end of hardship comes hapiness.
* At the end of hapiness comes another project.
* At the end of last project comes JongGang(Probably).

## P.S.
In this time, we wrote README very shortly for our great T.A.
Thank you so much.
