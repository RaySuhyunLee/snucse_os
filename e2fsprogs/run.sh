./configure
make
dd if=/dev/zero of=proj4.fs bs=1M count=1
sudo losetup /dev/loop0 proj4.fs
sudo ./e2fsprogs/misc/mke2fs -I 256 -L os.proj4 /dev/loop0
sudo losetup -d /dev/loop0a
