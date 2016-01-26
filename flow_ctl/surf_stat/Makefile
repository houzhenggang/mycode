#This is the makefile of surf_stat
#objects = surf_stat
#.PHONY:all
#all: $(objects)
#surf_stat: 
#	gcc -g surf_stat.c arbiter_handler.c data_stream.c data_aggregation.c data_sort.c h_cache.c common.c data_db.c log.c  data_test.c -o surf_stat -lpthread 
#clean : 
#	rm $(objects)   
#########################################
#########################################

#Source file
SRC = surf_stat.c arbiter_handler.c data_stream.c data_aggregation.c data_sort.c h_cache.c common.c data_db.c log.c  data_test.c
#Object file
OBJ = $(SRC:.cpp=.o)
#Output execution file
PROGRAM = surf_stat
#Compiler
CC = gcc -g


#Include
#INCLUDE = -I/usr/include/ncurses ¨CI/usr/include/
#Linker Parameter
LINKPARAM = -lpthread
#Options for development
CFLAGS = -g

#Options for release
CFLAGS += -o 


all: $(PROGRAM)

$(PROGRAM): $(OBJ)
	$(CC) -o $(PROGRAM) $(LINKPARAM) $(OBJ) 

.SUFFIXES:.cpp

.cpp.o:
	$(CC) $(INCLUDE) $(CFLAGS) -c $<

clean:
	-rm $(PROGRAM)
