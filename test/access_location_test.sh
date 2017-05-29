mkdir access_loc_test
echo "---------- Read Test ----------"
mount -o loop -t ext2 proj4.fs access_loc_test
echo "301동으로 이동, 오차 60m"
./gpsupdate.o 37.449930 126.952501 60
echo "create file 301"
echo "301" > access_loc_test/301

echo "302동으로 이동, 오차 60m"
./gpsupdate.o 37.448774 126.952551 60
echo "open file 301"
cat access_loc_test/301
echo "create file 302"
echo "302" > access_loc_test/302

echo "302동으로 이동, 오차 90m"
./gpsupdate.o 37.448774 126.952551 90
echo "open file 301"
cat access_loc_test/301

echo "301동 뒤뜰로 이동, 오차 60m"
./gpsupdate.o 37.449454 126.952494 60
echo "open file 301"
cat access_loc_test/301
echo "open file 302"
cat access_loc_test/302

echo "---------- Write Test ----------"
echo "301동 앞 도로로 이동, 오차 10m"
./gpsupdate.o 37.449894 126.951993 10
echo "write to file 301"
echo "301" > access_loc_test/301
echo "write to file 302"
echo "302" > access_loc_test/302

echo "성균관대 앞 CU로 이동, 오차 100m"
./gpsupdate.o 37.585829 126.996974 100
echo "write to file skku"
echo "skku" > access_loc_test/skku
echo "write to file 301"
echo "301" > access_loc_test/301

echo "---------- Delete Test ----------"
rm access_loc_test/*
