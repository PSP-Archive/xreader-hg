#32bit for fw 1.xx
./configure
make; make SCEkxploit
mkdir -p ./release/32bit/100
mkdir -p ./release/32bit/150
cp -rf EBOOT.PBP ./release/32bit/100/
cp -rf __SCE__eReader ./release/32bit/150/
cp -rf %__SCE__eReader ./release/32bit/150/
zip -9 -j ./release/32bit/100/fonts.zip ./fonts/GBK12 ./fonts/ASC12 ./fonts/GBK14 ./fonts/ASC14 ./fonts/GBK16 ./fonts/ASC16
cp -rf ./release/32bit/100/fonts.zip ./release/32bit/150/__SCE__eReader/
make clean

#16bit for fw 1.xx
./configure --enable-16bit
make; make SCEkxploit
mkdir -p ./release/16bit/100
mkdir -p ./release/16bit/150
cp -rf EBOOT.PBP ./release/16bit/100/
cp -rf __SCE__eReader ./release/16bit/150/
cp -rf %__SCE__eReader ./release/16bit/150/
zip -9 -j ./release/16bit/100/fonts.zip ./fonts/GBK12 ./fonts/ASC12 ./fonts/GBK14 ./fonts/ASC14 ./fonts/GBK16 ./fonts/ASC16
cp -rf ./release/16bit/100/fonts.zip ./release/16bit/150/__SCE__eReader/
make clean

#32bit for fw 2.xx
./configure --enable-fw2xx
make
mkdir -p ./release/32bit/2xx
cp -rf EBOOT.PBP ./release/32bit/2xx/
cp -rf ./release/32bit/100/fonts.zip ./release/32bit/2xx/
make clean

#16bit for fw 2.xx
./configure --enable-fw2xx --enable-16bit
make
mkdir -p ./release/16bit/2xx
cp -rf EBOOT.PBP ./release/16bit/2xx/
cp -rf ./release/16bit/100/fonts.zip ./release/16bit/2xx/
make clean
