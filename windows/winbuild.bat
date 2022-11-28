rem This is written to be used with TinyCC from here: https://github.com/cnlohr/tinycc-win64-installer/releases/tag/v0_0.9.27
rem You will need to download a kernel, here: https://github.com/cnlohr/mini-rv32ima-images/raw/master/images/linux-5.18.0-rv32nommu-cnl-1.zip

tcc ..\mini-rv32ima\mini-rv32ima.c
mini-rv32ima -f Image
