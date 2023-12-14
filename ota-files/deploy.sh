alias pio="~/.platformio/penv/bin/platformio"
cd ../code
pio run
cd ../ota-files
cp ../code/.pio/build/esp32-c3-devkitm-1/firmware.bin .
rsync -avz . sx6-store@93.114.184.165:public/bitkoclock
ssh sx6-store@93.114.184.165 chmod 0777 public/bitkoclock/firmware.bin
