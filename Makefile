CC = cl

# need c++latest for designated initializers
CFLAGS = /O2 /std:c++latest /EHsc
EXE_DIR = .
EXE_NAME = fuser-util.exe
SRC_DIR = .\src
MOGG_SRCS = \
	$(SRC_DIR)\mogg\aes.c \
	$(SRC_DIR)\mogg\CCallbacks.cpp \
	$(SRC_DIR)\mogg\OggMap.cpp \
	$(SRC_DIR)\mogg\oggvorbis.cpp \
	$(SRC_DIR)\mogg\VorbisEncrypter.cpp
SRCS = \
	$(SRC_DIR)\main.cpp \
	$(SRC_DIR)\SMF.cpp \
	$(SRC_DIR)\MidiFileResource.cpp \
	$(SRC_DIR)\HmxAsset.cpp \
	$(SRC_DIR)\stream-helpers.cpp \
	$(SRC_DIR)\Data.cpp \
	$(MOGG_SRCS)

$(EXE_NAME): $(SRCS)
	$(CC) /Fe: $(EXE_DIR)\$(EXE_NAME) $(SRCS) $(CFLAGS)

clean:
	del $(EXE_DIR)\$(EXE_NAME)
	del *.obj