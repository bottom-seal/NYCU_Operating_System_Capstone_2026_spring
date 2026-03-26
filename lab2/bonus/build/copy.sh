#!/bin/bash

set -e

DEVICE="/dev/sdb1"
MOUNT_POINT="/mnt"
FILE="kernel.fit"

echo "Mounting $DEVICE to $MOUNT_POINT..."
sudo mount "$DEVICE" "$MOUNT_POINT"

echo "Copying $FILE to $MOUNT_POINT..."
sudo cp "$FILE" "$MOUNT_POINT/$FILE"

echo "Flushing writes..."
sync

echo "Unmounting $MOUNT_POINT..."
sudo umount "$MOUNT_POINT"

echo "Done."
