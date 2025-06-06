# 检测 conda 环境路径（也可以写死为 /mnt/driver_g/users/usr6/.conda/envs/test）
CONDA_PREFIX ?= $(shell echo $$CONDA_PREFIX)

# -I 是给编译器添加头文件搜索路径（比如 zstd.h）
INCLUDE_DIRS := -Iinc -I$(CONDA_PREFIX)/include

# -L 是给链接器添加库文件搜索路径（比如 libzstd.a）
LIB_DIRS := -Llib -L$(CONDA_PREFIX)/lib

# 优化和调试选项
ifeq ($(MODE), DEBUG)
CFLAG += -g -fsanitize=address
else 
CFLAG += -O3 -DNDEBUG
endif

.PHONY: clean

# Compile Rules (Dependencies relationship)
# 表示可执行文件 compression_test 由两个 .o 文件和一堆静态库组成
compression_test: lib/libmach.a lib/libgorilla.a lib/libchimp.a lib/libelf.a lib/liblfzip.a
compression_test: compression_test.o wrapper.o 
	$(CXX) $(CFLAG) $(LIB_DIRS) $^ -lmach -lgorilla -lchimp -lelf -llfzip -lSZ3c -lbsc -fopenmp -lzstd -lz -o $@
# $^ 表示所有依赖目标（这里是 .o 文件）
# -lxxx 表示链接静态库 libxxx.a
# -o $@ 表示输出文件名是 compression_test




# 子模块静态库
lib/libmach.a:  
	$(MAKE) -C machete/ CFLAG="$(CFLAG)"
	cp machete/libmach.a lib 

# 通配规则：如果你写了 lib/libchimp.a 这个目标，它就会跳到 chimp/ 子目录，执行 make，然后拷贝结果到 lib/
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
