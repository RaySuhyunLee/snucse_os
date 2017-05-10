if [ $# -eq 2 ]
then
	echo "Start test with $2 processes"
	for i in $(seq 1 $2);
	do
		$1 &
	done
fi
