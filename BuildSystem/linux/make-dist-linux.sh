#!/bin/bash

source "./plugins-list.sh"
source "./plugs-path.sh"

BUILD_SYS_DIR=$(pwd)

#export CMAKE_BUILD_PARALLEL_LEVEL=1
#export CMAKE_BUILD_PARALLEL_LEVEL=2
#export CMAKE_BUILD_PARALLEL_LEVEL=3

export CMAKE_BUILD_PARALLEL_LEVEL=8

for PLUG in $PLUGS
do
    PLUG_PATH=$PLUGS_PATH/$PLUG

    echo $PLUG
    
    cd $PLUG_PATH/scripts
    
    source ./makedist-linux.sh 0
    
    #sleep 30

    cd $BUILD_SYS_DIR
done
