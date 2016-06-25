ipclean
i=0
while [ $i -le 20 ]
do
	echo "---------------------------test " $i
	rm -f ~/Data/dbc/sync/*
	dbc_server &
	pid=$!
	sleep 1
	dbc_test -u xugq -p 123 -i $i
	kill -9 $pid
	ipclean
	i=`expr $i + 1`
done 2> /dev/null
