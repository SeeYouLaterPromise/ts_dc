# 检测 conda 环境路径（也可以写死为 /mnt/driver_g/users/usr6/.conda/envs/test）
CONDA_PREFIX ?= $(shell echo $$CONDA_PREFIX)

INCLUDE_DIRS := -Iinc -I$(CONDA_PREFIX)/include
LIB_DIRS := -Llib -L$(CONDA_PREFIX)/lib

# 优化和调试选项
ifeq ($(MODE), DEBUG)
CFLAG += -g -fsanitize=address
else 
CFLAG += -O3 -DNDEBUG
endif

.PHONY: clean

# 主程序目标
compression_test: lib/libmach.a lib/libgorilla.a lib/libchimp.a lib/libelf.a lib/liblfzip.a
compression_test: compression_test.o wrapper.o 
	$(CXX) $(CFLAG) $(LIB_DIRS) $^ -lmach -lgorilla -lchimp -lelf -llfzip -lSZ3c -lbsc -fopenmp -lzstd -lz -o $@

# 子模块静态库
lib/libmach.a:  
	$(MAKE) -C machete/ CFLAG="$(CFLAG)"
	cp machete/libmach.a lib 

lib/lib%.a:
	$(MAKE) -C $* CFLAG="$(CFLAG)"
	cp $*/lib$*.a lib

# 编译 .cpp -> .o，加入 include 路径
%.o: %.cpp
	$(CXX) -c $(CFLAG) $(INCLUDE_DIRS) $< -o $@

# 清理命令
clean:
	cd machete && make clean 
	cd lfzip && make clean
	cd gorilla && make clean 
	cd chimp && make clean 
	cd elf && make clean
	rm -f tmp* compression_test *.o
	rm -f lib/libmach.a lib/libgorilla.a lib/libchimp.a lib/libelf.a lib/liblfzip.a
