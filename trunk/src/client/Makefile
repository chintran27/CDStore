CC = g++
CFLAGS = -O3 -Wall -fno-operator-names
LIBS = -lcrypto -lssl -lpthread 
INCLUDES =-I./lib/cryptopp -I./comm -I./coding -I./chunking -I./utils
MAIN_OBJS = ./chunking/chunker.o ./utils/CryptoPrimitive.o ./coding/CDCodec.o ./coding/encoder.o ./comm/uploader.o ./utils/socket.o ./comm/downloader.o ./coding/decoder.o 

all: client

%.o: %.cc %.hh
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

client: ./main.cc $(MAIN_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o CLIENT ./main.cc $(MAIN_OBJS) ./lib/gf_complete.a  $(LIBS)

clean:
	@rm -f CLIENT
	@rm -f $(MAIN_OBJS)
