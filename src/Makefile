CPPFLAGS = -I../include
CFLAGS = -Wall
LDFLAGS =  -lpthread
#CC = gcc
CC = g++

ifeq ($(CC),gcc)
	src = $(wildcard *.c)
	obj = $(patsubst %.c,%.o,$(src))
else
	src = $(wildcard *.cpp)
	obj = $(patsubst %.cpp,%.o,$(src))
endif
target = app
$(target):$(obj)
	    $(CC) $(obj) $(LDFLAGS) -o $@
ifeq ($(CC),gcc)
%.o:%.c
else
%.o:%.cpp
endif
	$(CC) -c $< $(CFLAGS) $(CPPFLAGS) -o $@

-PHONY:clean
clean:
	-rm -f *.o
	-rm -f $(target)
	-rm -rf tags

ctags:
	ctags -R
