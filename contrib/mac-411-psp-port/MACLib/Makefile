PSPSDK=$(shell psp-config --pspsdk-path)
PSPDIR=$(shell psp-config --psp-prefix)
TARGET_LIB = libmac.a
INCDIR = ../Shared
OBJS = APEDecompress.o \
	   Prepare.o \
	   MD5.o \
	   UnBitArray.o \
	   UnBitArrayBase.o \
	   APELink.o \
	   APEInfo.o \
	   APETag.o \
	   APEHeader.o \
	   MACLib.o \
	   NewPredictor.o \
	   NNFilter.o \
	   Old/Anti-Predictor.o \
	   Old/AntiPredictorHigh.o \
	   Old/AntiPredictorExtraHigh.o \
	   Old/AntiPredictorFast.o \
	   Old/AntiPredictorNormal.o \
	   Old/APEDecompressCore.o \
	   Old/APEDecompressOld.o \
	   Old/UnBitArrayOld.o \
	   Old/UnMAC.o \
	   ../Shared/GlobalFunctions.o \
	   ../Shared/CircleBuffer.o \
	   ../Shared/CharacterHelper.o \
	   PSP.o \
	   ../Shared/StdLibFileIO.o \

CXXFLAGS = -O2 -G0 -I $(psp-config -p)/include -DBUILD_CROSS_PLATFORM -fno-rtti
PSP_FW_VERSION=639

include $(PSPSDK)/lib/build.mak
