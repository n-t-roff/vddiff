#!/bin/sh

TEST_DIR_NAME="$1"
mkdir $TEST_DIR_NAME || exit 1
cd $TEST_DIR_NAME || exit 1
touch "Empty regular file"
ln -s "Empty regular file" "Symlink to empty regular file"
ln -s "Non-existing file" "Dead link"
echo "123" > "File 1"
echo "456" > "File 2"
cp "File 1" "Copy of file 1"
mkdir Directory
cd Directory
ln -s File Link
