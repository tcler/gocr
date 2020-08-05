#!/bin/bash
#  convert -list font | grep Font
#  convert -list font | grep "^  Font:" | grep Mono # 
font="FreeMono-Regular"        # PC16.CentOS7
font="Droid-Sans-Mono-Regular" # 12hpmini.deb-7.11
font="DejaVu-Sans-Mono-Book"   # PC16.CentOS7
# 
if [ -r rnd80.png.txt ];then true; else
  dd if=/dev/urandom bs=285 count=1 status=noxfer 2>/dev/null\
   | base64 > rnd80.png.txt
fi
convert -size 3800x600 xc:white -pointsize 80 -fill black\
  -font "$font"\
  -draw "text 2,80  \"$(head -1 <rnd80.png.txt)\""\
  -draw "text 2,160 \"$(head -2 <rnd80.png.txt| tail -1)\""\
  -draw "text 2,240 \"$(head -3 <rnd80.png.txt| tail -1)\""\
  -draw "text 2,320 \"$(head -4 <rnd80.png.txt| tail -1)\""\
  -draw "text 2,400 \"$(head -5 <rnd80.png.txt| tail -1)\""\
  PNG8:rnd80-$font.png
cp rnd80.png.txt rnd80-$font.png.txt
identify rnd80-$font.png 
# display -geometry 600x400 out30.png
              
# rnd80-Droid-Sans-Mono-Regular.png      236KB 4*16bit/pixel
#  -> pngtopnm P5 65535=16bit gray 4.4MB (3800*600=2.280M * 16bit)
#  xy=452,203  89 249 255   Endianess.NetPBM=MSB most sig. byte first
# rnd80-Droid-Sans-Mono-Regular_8bit.png 121KB 3*8bit/pixel
#  -> pngtopnm P5   255=8bit  gray 2.2MB (3800*600=2.280M * 8bit)
#  xy=452,203  90 248 255 
