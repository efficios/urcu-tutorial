#!/bin/sh

TOP_DIR=$(pwd)
GAME_DIR=${TOP_DIR}/../urcu-game

mkdir ${TOP_DIR}/questions/
mkdir ${TOP_DIR}/answers/

for a in $(cat exercises.list); do
	mkdir ${TOP_DIR}/${a}
	cp ${GAME_DIR}/*.[ch] ${TOP_DIR}/${a}
	cp ${GAME_DIR}/Makefile ${TOP_DIR}/${a}
	cd ${TOP_DIR}/${a}
	if [[ -f ${TOP_DIR}/patches/${a}.patch ]] ; then
		patch -p2 < ${TOP_DIR}/patches/${a}.patch
	fi
done
