#!/bin/bash

find . -name '*.c' -exec grep -lP '\t$' {} \;
find . -name '*.h' -exec grep -lP '\t$' {} \;
