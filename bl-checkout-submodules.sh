#!/bin/sh

# bluelabpxx@ssh.cluster026.hosting.ovh.net:bluelab.git

# git submodules init
# git submodules update

git pull --recurse-submodules

cd iPlug2
git pull origin bluelab
git checkout bluelab
cd ..

cd Libs

cd darknet
git checkout bluelab
git pull origin bluelab
cd ..

cd flac
git checkout bluelab
git pull origin bluelab
cd ..

cd fmem
git checkout bluelab
git pull origin bluelab
cd ..

cd freeverb
git checkout master
git pull origin master
cd ..

cd glm
git checkout bluelab
git pull origin bluelab
cd ..

cd libebur128
git checkout bluelab
git pull origin bluelab
cd ..

cd OpenBLAS
git checkout bluelab
git pull origin bluelab
cd ..

cd RandomSequence
git checkout bluelab
git pull origin bluelab
cd ..

cd RTConvolve
git checkout bluelab
git pull origin bluelab
cd ..

cd sndfilter
git checkout bluelab
git pull origin bluelab
cd ..

cd ..
