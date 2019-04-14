#!/bin/bash

ps ax | grep -v a.out | tail -n +2 | awk '{print $1}' | xargs -I {} taskset -cp 0 {}

PIDS=$(sudo ps ax | grep a.out | grep -v sudo | grep -v grep | awk '{print $1}')
echo $PIDS

I=1
while read line; do
    taskset -cp $I $line
    I=$(($I+1))
done <<< $PIDS
