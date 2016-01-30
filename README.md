
# README #

This repo shows a stripped down version of the official nvidia shield tv open source package
(ref: https://developer.nvidia.com/shield-open-source) which still allows building a bootimage 
(=KERNEL+initial ramdisk). 

Depending on the branch and revision, I have added some features like NTFS file system support,
full Digital TV (DVB-C/C2/S/S2/T/T2) drivers etc, please check the commit log for a description 
on what has been done yet.

If you want to start from scratch (=unmodified kernel as in the nvidia offical distribution), 
update to the very first commit (id ad7fa81).


### How to compile? ###
```
#!bash

cd shield-open-source
export TOP=`pwd`
. build/envsetup.sh 
setpaths
lunch foster_e-userdebug # (use "lunch foster_e_hdd-userdebug" if you got the shield tv pro)
mp bootimage -j9 # (change j9 to j<number-of-cpu-cores+1> for optimal build speed)

``` 

### Anything else? ###
Check the official website http://nstvdev.nopnop9090.com (which should redirect you to the 
most recent repo) for updates and how-to guides (-> Wiki).