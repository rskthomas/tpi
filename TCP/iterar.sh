#!/bin/bash

IP_ADDRESS="172.30.64.1"
PORT="1234"

for i in {0..4}; do
  INPUT_FILE="text_$i.txt"
  OUTPUT_FILE="output_$i.txt"

  
  # Corre cada cliente 5 veces y guarda el output en un archivo
  for i in {0..4}; do
  ./client "$IP_ADDRESS" "$PORT" "$INPUT_FILE" >> "$OUTPUT_FILE"
  sleep 10
  done

  echo "Envío de archivos $INPUT_FILE finalizado"
done
