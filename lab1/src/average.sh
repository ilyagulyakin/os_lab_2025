#!/bin/bash

count=$#
sum=0

for num in "$@"; do
    sum=$((sum + num))
done

average=$((sum / count))

echo "Количество чисел: $count"
echo "Среднее арифметическое: $average"