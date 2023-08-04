#!/bin/bash

PRJ_NAME='test03'
PRJ_DIR='../'$PRJ_NAME'/'
DEST_DIR='../build_backups/'

DATE=$(date +%F)
TIME=$(date +%T)

DEST_FILE_PREFIX='MDT_Ebaz_image_'$PRJ_NAME'_'
DEST_FILE_DATE_TIME=$DATE'_'$TIME
DEST_FILE_EXT='.tar.gz'

DEST_PATH=$DEST_DIR$DEST_FILE_PREFIX$DEST_FILE_DATE_TIME$DEST_FILE_EXT

BOOT_BIN=$PRJ_DIR'images/linux/BOOT.BIN'
BOOT_SCR=$PRJ_DIR'images/linux/boot.scr'
U_IMAGE=$PRJ_DIR'images/linux/uImage'
IMAGE_UB=$PRJ_DIR'images/linux/image.ub'
ROOT_FS=$PRJ_DIR'images/linux/rootfs.tar.gz'

tar -czvf $DEST_PATH $BOOT_BIN $BOOT_SCR $U_IMAGE $IMAGE_UB $ROOT_FS