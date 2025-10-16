# Makefile for DPU application

######################################################
TARGET = main
######################################################
# Host source files
HOST_SRC = main.cpp db.cpp fpgrowth.cpp
HOST_OBJ = $(addprefix build/, $(HOST_SRC:.cpp=.o))
######################################################
# DPU source files
DPU_SRC = db_count_item.c mine_candidates.c
######################################################

HOST_CC = g++
HOST_CFLAGS = -O2 -std=gnu++20 -Wall -Wextra -Werror -pthread
HOST_LDFLAGS = `dpu-pkg-config --cflags --libs dpu`

DPU_CC = dpu-upmem-dpurte-clang
DPU_CFLAGS = -O2 -DNR_TASKLETS=16 -DSTACK_SIZE_DEFAULT=256

HOST_BIN = build/$(TARGET)
DPU_BIN = $(addprefix build/, $(basename $(DPU_SRC)))

INCLUDE_DIR = ./include

.PHONY: all clean

all: build/ $(HOST_BIN) $(DPU_BIN)

# build 디렉토리 생성
build/:
	mkdir -p build

$(HOST_OBJ): build/%.o: host/%.cpp | build/
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_LDFLAGS) -c $< -o $@ -I $(INCLUDE_DIR)

$(HOST_BIN): $(HOST_OBJ) | build/
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $(HOST_OBJ) $(HOST_LDFLAGS)

# DPU 소스 파일들을 자동으로 개별 실행파일로 컴파일
$(DPU_BIN): build/%: dpu/%.c | build/
	$(DPU_CC) $(DPU_CFLAGS) -o $@ $< -I $(INCLUDE_DIR)

clean:
	rm -rf build/
