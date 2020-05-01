#!/usr/bin/make -f

BINARY= OgreMeshMagick

CC= gcc
CXX= g++
RM= rm -f

PKGCONFIG= pkg-config
PACKAGES= OGRE

CFLAGS= \
	-Wall \
	-fstack-protector-strong \
	-Wall \
	-Wformat \
	-Werror=format-security \
	-Wdate-time \
	-D_FORTIFY_SOURCE=2 \
	$(shell $(PKGCONFIG) --cflags $(PACKAGES))

LDFLAGS= \
	-Wl,--as-needed \
	-Wl,--no-undefined \
	-Wl,--no-allow-shlib-undefined

CSTD=-std=c11
CPPSTD=-std=c++11

OPTS= -O2 -g
DEFS=
INCS= -Iinclude
LIBS= $(shell $(PKGCONFIG) --libs $(PACKAGES))

BINARY_SRCS= \
	src/main.cpp \
	src/MmEditableMesh.cpp \
	src/MmInfoToolFactory.cpp \
	src/MmMeshUtils.cpp \
	src/MmOptimiseToolFactory.cpp \
	src/MmRenameToolFactory.cpp \
	src/MmTool.cpp \
	src/MmTransformTool.cpp \
	src/MeshMagick.cpp \
	src/MmEditableSkeleton.cpp \
	src/MmMeshMergeTool.cpp \
	src/MmOgreEnvironment.cpp \
	src/MmOptionsParser.cpp \
	src/MmStatefulMeshSerializer.cpp \
	src/MmToolManager.cpp \
	src/MmTransformToolFactory.cpp \
	src/MmEditableBone.cpp \
	src/MmInfoTool.cpp \
	src/MmMeshMergeToolFactory.cpp \
	src/MmOptimiseTool.cpp \
	src/MmRenameTool.cpp \
	src/MmStatefulSkeletonSerializer.cpp \
	src/MmToolsUtils.cpp

BINARY_OBJS= $(subst .cpp,.o,$(BINARY_SRCS))

all: $(BINARY)

$(BINARY): $(BINARY_OBJS)
	$(CXX) $(CPPSTD) $(CSTD) $(LDFLAGS) -o $@ $(BINARY_OBJS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CPPSTD) $(OPTS) -o $@ -c $< $(DEFS) $(INCS) $(CFLAGS)

%.o: %.cc
	$(CXX) $(CPPSTD) $(OPTS) -o $@ -c $< $(DEFS) $(INCS) $(CFLAGS)

%.o: %.c
	$(CC) $(CSTD) $(OPTS) -o $@ -c $< $(DEFS) $(INCS) $(CFLAGS)

depend: .depend

.depend: $(BINARY_SRCS)
	$(RM) ./.depend
	$(CXX) $(CPPSTD) $(DEFS) $(INCS) $(CFLAGS) -MM $^>>./.depend;

clean:
	$(RM) $(BINARY_OBJS) $(BINARY)
	$(RM) -fv *~ .depend core *.out *.bak
	$(RM) -fv *.o *.a *~
	$(RM) -fv */*.o */*.a */*~

include .depend

.PHONY: all depend clean
