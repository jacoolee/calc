# Makefile for calculator

TARGET = calc

SRCS += $(wildcard *.cc)
OBJS = $(foreach src, $(SRCS), $(basename $(src)).o )

CXX = g++
CFLAGS += -pipe -Wall -g


################################################################
.PHONY : all clean
all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) -$(CFLAGS) -o $@ $(OBJS)

clean :
	@rm -f $(TARGET) *.o

################################################################
.SUFFIXES : .cc
.cc.o :
	$(CXX) $(CFLAGS) -c -o $@ $<
