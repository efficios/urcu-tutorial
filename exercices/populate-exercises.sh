#!/bin/sh

TOP_DIR=$(pwd)
GAME_DIR=${TOP_DIR}/../urcu-game

for a in $(cat exercises.list); do
	mkdir ${TOP_DIR}/questions/${a}
	cp ${GAME_DIR}/*.[ch] ${TOP_DIR}/questions/${a}
	cp ${GAME_DIR}/Makefile ${TOP_DIR}/questions/${a}
	cd ${TOP_DIR}/questions/${a}
	patch -p2 < ${TOP_DIR}/patches/${a}.patch
done
