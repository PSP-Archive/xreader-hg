#!/bin/sh

cd $(dirname $0)/../fonts
rm -f fonts.zip
zip fonts.zip ASC10 ASC12 ASC14 ASC16 ASC18 GBK10 GBK12 GBK14 GBK16 GBK18
#zip fonts.zip ASC12 GBK12
