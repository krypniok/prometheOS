#!/bin/bash
while IFS= read -r line; do
  printf '%s
' "$line"
done
