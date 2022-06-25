#! /bin/sh

#remove the plugins in the installation directories "/Library ..."

source "./plugins-list.sh"
source "./plug-path.sh"

for plug in $PLUG_LIST
do
    PLUG_PATH=$PLUGS_PATH/$plug

    cd $PLUG_PATH

    echo $PLUG_PATH
    
    source ./make-clean-install.command
done
