# bluelab-plugins
All BlueLab plugins and apps

All the necessary files to rebuild all the plugins, installers, modify the design and the documentation.

There are interesting plugins such as phantom fundamental effect, good quality stero widening, good quality pitch shifting, spectrogram edition, experimental volume rendering of sound, Deep Learning demixing, visualizers, partials tracking, experimental (not finished) synthetizer based on the SAS model (Strutured Additive Synthesis)...

The plugins can be built on Linux, MacOS and Windows (vst2/vst3/au/aax and app).

To see them in action, have a look at [these videos](https://www.youtube.com/channel/UCh1HekQ6Xzih3NRmHRYaWlg).

## Checking out
git clone --recursive https://github.com/deadlab-plugins/bluelab-plugins.git

## Building on Linux
- call bl-checkout-submodules.sh
- call bl-replace-nanovg.sh
- edit plugs-path.sh and set the path for your system
- launch the script make-links.sh
- cd iPlug2/Dependencies/IPlug and launch download-iplug-sdks.sh

     Build flac
     - cd Libs/flac
     - ./autogen.sh
     - ./configure
     - make
     - mkdir lib/linux
     - cp ./src/libFLAC/.libs/libFLAC-static.a lib/linux

- cd BuildSystem/linux and call make-dist-linux.sh
(change plugins-list.sh to select only some plugins to build)

![bluelab plugins](https://github.com/deadlab-plugins/bluelab-plugins/blob/master/Images/bluelab-plugins.png)