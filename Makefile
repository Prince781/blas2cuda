# put /opt/cuda/bin/ into $PATH
NVCC=nvcc
comma:= ,
empty:=
space:=$(empty) $(empty)
CUDA ?= /opt/cuda
NVSOURCES=$(wildcard *.cu) $(wildcard blas_level1/*.cu) $(wildcard blas_level2/*.cu) $(wildcard blas_level3/*.cu)
CSOURCES=$(wildcard *.c) $(filter-out lib/blas_tracker.c, $(filter-out $(wildcard tests/*.c),$(wildcard */*.c)))
CXXSOURCES=$(wildcard *.cc) $(wildcard blas_level1/*.cc) $(wildcard blas_level2/*.cc) $(wildcard blas_level3/*.cc)
OBJDIR=obj
OBJECTS=$(wildcard $(OBJDIR)/*.o)
LIBDIR=lib
COMPFLAGS = -Wall -Werror -fPIC -fdiagnostics-color -ggdb3 -I$(CUDA)/include
CFLAGS += $(COMPFLAGS) -std=gnu11
CXXFLAGS += $(COMPFLAGS) -std=gnu++11
NVCFLAGS=$(subst $(space),$(comma),$(CFLAGS))
LDFLAGS += -L$(CUDA)/lib64 -lcublas -L$(LIBDIR) -ldl -lunwind -lunwind-x86_64
NVLDFLAGS=$(subst $(space),$(comma),$(LDFLAGS))

libblas2cuda.so: $(NVSOURCES:%.cu=$(OBJDIR)/%.o) $(CSOURCES:%.c=$(OBJDIR)/%.o) $(CXXSOURCES:%.cc=$(OBJDIR)/%.o)
	$(NVCC) -shared -Xlinker $(NVLDFLAGS) $^ -o $@

$(OBJDIR)/%.o: %.c
	@if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -c $^ -o $@

$(OBJDIR)/%.o: %.cu cblas.h
	@if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
	$(NVCC) -Xcompiler $(NVCFLAGS) -shared -c $< -o $@

$(OBJDIR)/%.o: %.cc cblas.h
	@if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
	$(CXX) $(CXXFLAGS) -shared $(LDFLAGS) -c $< -o $@

#$(LIBDIR)/libobjtracker.so:
#	$(MAKE) -C $(LIBDIR) libobjtracker.so

.PHONY: clean tests

tests:
	$(MAKE) -C tests/

clean:
	rm -rf $(OBJDIR)
	rm -f libblas2cuda.so
	@#@$(MAKE) -C $(LIBDIR) clean
