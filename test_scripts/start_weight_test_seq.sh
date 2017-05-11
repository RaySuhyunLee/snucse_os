if [ $# -eq 1 ]
then
	echo "Start test with 20 diffrent weights"
	for i in $(seq 1 20);
	do
		$1 $i
	done
fi
