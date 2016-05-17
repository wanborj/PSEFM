#!/bin/bash

OrderOfTask=5;
NumOfConcurrents=7;

function ave(){
awk '
BEGIN{
    Time = 0;
    id = 0;
    count = 0;
    total = 0;
    max = 0;
    min =100000;
}
NR>108{
    if( id == 0 && Time == 0 && $1 < 200 ){
        id = $1;
    }else if( id != 0 && Time == 0 && $1 != (id+10)*3 ){
        Time = $2/1000.0;
    }else if( id != 0 && Time != 0 && $1 != (id+10)*3 ){
        count ++;
        tmp = ($2/1000.0 - Time);
        total += tmp;
        if( max < tmp){
            max = tmp;
        }
        if( min > tmp){
            min = tmp;
        }
    }else{
        Time = 0;
        id = 0;
    }
}
END{
    print "'$1'","'$2'";
    print count, (total/count)/3;
}
' ./PSEFM_DATA/$1_$2.data 
}

for i in `seq 0 $OrderOfTask`
do
    for j in `seq 0 $NumOfConcurrents`
    do
        if [ -e ./PSEFM_DATA/$[2**$i]_$j.data ]
        then
            ave $((2**$i)) $j;
        fi 
    done
done


