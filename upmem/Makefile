# Makefile for DPU application

######################################################
TARGET = main
######################################################
# Host source files
HOST_SRC = main.cpp db.cpp fpgrowth.cpp
HOST_OBJ = $(HOST_SRC:.cpp=.o)
######################################################
# DPU source files
DPU_SRC = db_count_item.c
DPU_OBJ = $(DPU_SRC:.c=.o)
######################################################

HOST_CC = g++
HOST_CFLAGS = -g
HOST_LDFLAGS = `dpu-pkg-config --cflags --libs dpu`

DPU_CC = dpu-upmem-dpurte-clang
DPU_CFLAGS = -O0 -g

HOST_BIN = $(TARGET)
DPU_BIN = $(basename $(DPU_SRC))

INCLUDE_DIR = ./include

.PHONY: all clean

all: $(HOST_BIN) $(DPU_BIN)

$(HOST_OBJ): %.o: host/%.cpp
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_LDFLAGS) -c $< -o $@ -I $(INCLUDE_DIR)
$(DPU_OBJ): %.o: dpu/%.c
	$(DPU_CC) $(DPU_CFLAGS) -c $< -o $@ -I $(INCLUDE_DIR)
$(HOST_BIN): $(HOST_OBJ)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $(HOST_OBJ) $(HOST_LDFLAGS)
$(DPU_BIN): $(DPU_OBJ)
	$(DPU_CC) $(DPU_CFLAGS) -o $@ $(DPU_OBJ)


clean:
	rm -f *.o $(HOST_BIN) $(DPU_BIN)
