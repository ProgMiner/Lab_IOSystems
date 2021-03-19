#!/bin/bash

if [ "$(whoami)" != "root" ]; then
  sudo "$0" "$@"
  exit $?
fi

mkfs.vfat /dev/lab2p1
mkfs.vfat /dev/lab2p5
mkfs.vfat /dev/lab2p6

mkdir /mnt/disk1
mkdir /mnt/disk5
mkdir /mnt/disk6

mount /dev/lab2p1 /mnt/disk1
mount /dev/lab2p5 /mnt/disk5
mount /dev/lab2p6 /mnt/disk6

function create_files() {
  dd if=/dev/urandom of=/mnt/disk1/file bs=1M count=5
  dd if=/dev/urandom of=/mnt/disk5/file bs=1M count=5
  dd if=/dev/urandom of=/mnt/disk6/file bs=1M count=5
}

function delete_files() {
  rm /mnt/disk1/file
  rm /mnt/disk5/file
  rm /mnt/disk6/file
}

create_files

  echo "Copying files within virtual disk"
  pv /mnt/disk1/file > /mnt/disk6/file
  pv /mnt/disk5/file > /mnt/disk6/file
  pv /mnt/disk6/file > /mnt/disk1/file

delete_files

create_files
  echo "Copying files from virtual file to real disk"
  mkdir /tmp/io
  pv /mnt/disk1/file > /tmp/io/testfile
  pv /mnt/disk5/file > /tmp/io/testfile
  pv /mnt/disk6/file > /tmp/io/testfile
delete_files

umount /dev/lab2p1
umount /dev/lab2p5
umount /dev/lab2p6
