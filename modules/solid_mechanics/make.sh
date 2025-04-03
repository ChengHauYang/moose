#!/bin/bash

# 🧹 Unset environment variables that may cause header conflicts
unset CPATH
unset CPLUS_INCLUDE_PATH

# Print current values (should be empty)
echo "CPATH = $CPATH"
echo "CPLUS_INCLUDE_PATH = $CPLUS_INCLUDE_PATH"

# Clean the build directory
echo "Cleaning build..."
make clean

# Rebuild with 8 threads
echo "Starting build with -j8 ..."
make -j8

