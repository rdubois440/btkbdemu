# btkbdemu - Bluetooth Keyboard Emulation on a Raspberry PI using OpenWrt

##1- Clone Chaos Calmer from github directory
```
cd SomeDirectory 
git clone git://git.openwrt.org/15.05/openwrt.git
cd openwrt
make menuconfig
```
Target --> Broadcom BCM2708 / BCM2835

Leave the rest as default. Target profile should be RaspberryPi

Save and quit
`make`

Be patient ...

##2- Update the feeds. 

packages bluez-libs and bluez-utils must be updated from the feeds

```
./scripts/feeds update -a
./scripts/feeds install bluez-libs
./scripts/feeds install bluez-utils
```



make menuconfig   
Enable bluez-libs and bluez-utils  
Kernel Modules --> Other Modules. Select kmod-bluetooth    

Deactivate dnsmasq and odhcp   
Base System --> deactivate dnsmasq   
Network --> deactivate odhcpd and odhcp6c   

Save and quit

`make`



##3- Create the custom package

Create the directory

```
mkdir /opt/openwrt/package/btkbdemu/src
cd /opt/openwrt/package/btkbdemu/src
```

Clone the source files to the new directory


`git clone https://github.com/rdubois440/btkbdemu.git .`

Cloning into '.'...

remote: Counting objects: 12, done.   
remote: Compressing objects: 100% (10/10), done.  
remote: Total 12 (delta 0), reused 9 (delta 0), pack-reused 0  
Unpacking objects: 100% (12/12), done.  
Checking connectivity... done.


Double check that the files exist, and can be built

`ls`

a.out    
btkbdemu.c     
hid.h     
Makefile     
README.md     
sdp.h     

Test the build on your local plaform

`make` 

cc -Wall   -c -o btkbdemu.o btkbdemu.c  
cc -Wall -lbluetooth btkbdemu.o  -o btkbdemu   
Cleanup the files created by the make

`make clean`

rm -f btkbdemu  *.o 

Move the special Makefile.openwrt to the parent directory

`mv Makefile.openwrt ../Makefile`

Check the last line, it should contain references to explicit libraries   
$(eval $(call BuildPackage,btkbdemu,+libusb,+libbluetooth))

See also the line specifying a dependency to the bluez-utils package  
DEPENDS:=bluez-utils



At this point, the new package should be available in openwrt build
`make menuconfig` 

Select Utilities --> btkbdemu Activate it  
`make`  
Make sure the build terminates without error  

##4- Create the SDCard

`cd /bin/brcm2708`   
`ls -l *.img`

-rw-r--r-- 1 rene rene 79691776 Aug 17 10:07 openwrt-brcm2708-bcm2708-sdcard-vfat-ext4.img   
Notice the small size, 80 Mb

su to root, copy the image to your SDCard.    

**Make sure to use the correct target device ! ! !    
/dev/YourSDCard will be deleted and cannot be recovered ! ! !** 

In my case, I use a microsd card on an SDCard adapter, my target is `of=/dev/mmcblk0`  
`sudo dd if=openwrt-brcm2708-bcm2708-sdcard-vfat-ext4.img of=/dev/YourSDCard bs=1M`  

76+0 records in  
76+0 records out  
79691776 bytes (80 MB) copied, 2.66479 s, 29.9 MB/s  

##5- Check it

Insert the SDCard in the Raspi, and boot it

Access the Raspi from the network, or from rs232

`ssh root@xxxxxxxxxx`


Configure the network for dhcp client   
the file /etc/config/network, lan section, should look like this   

```
config interface 'lan'
        option ifname 'eth0'
        option proto 'dhcp'
```


Make sure bluetooth is started and available. pscan and iscan visibility are not required   
if not started, start bluetooth with   


`root@OpenWrt:~/rene# hciconfig hci0`

```
hci0:   Type: USB
        BD Address: 00:0D:88:8E:E8:49 ACL MTU: 192:8 SCO MTU: 64:8
        UP RUNNING 
        RX bytes:2580 acl:32 sco:0 events:80 errors:0
        TX bytes:881 acl:22 sco:0 commands:49 errors:0
```



`btkbdemu -c FC:19:10:FE:DE:9F`

**PAIR THE DEVICES - see separate instructions   
 
