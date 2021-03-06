# parameter
.PHONY: all, clean  
   
# output target
TARGET = main  
   
# header files
HDRS += 
# C source files 
CSRCS += 
# C++ source files
CPPSRCS += main.cpp 

# object build directory
OBJSDIR = .

# object files
OBJS = $(patsubst %.c, $(OBJSDIR)/%.o, $(CSRCS))  
OBJS += $(patsubst %.cpp, $(OBJSDIR)/%.o, $(CPPSRCS))

# compile flag & linking flag   
CFLAGS += -std=c++11 -g -Wall 
LDFLAGS += -lgstapp-1.0 -lgio-2.0

CFLAGS += `pkg-config --cflags gstreamer-1.0 gstreamer-rtsp-server-1.0 opencv`
LDFLAGS += `pkg-config --libs gstreamer-1.0 gstreamer-rtsp-server-1.0 opencv`
   
# compiler
CC:= gcc  
CXX:= g++  
   
# default make
all: ${TARGET}  

# link object files
${TARGET} : $(OBJS)  
	@echo " [LINK] $@"  
	@mkdir -p $(shell dirname $@) 
	@$(CXX) $(OBJS) -o $@ $(LDFLAGS)   

# compile into object files
$(OBJSDIR)/%.o: %.c $(HDRS)  
	@echo " [CC] $@ $< $* $?"  
	@mkdir -p $(shell dirname $@)  
	@$(CC) -c $< -o $@ $(CFLAGS) 
   
$(OBJSDIR)/%.o: %.cpp $(HDRS)  
	@echo " [CXX] $@ $< $* $?"  
	@mkdir -p $(shell dirname $@)  
	@$(CXX) -c $< -o $@ $(CFLAGS)  

# clean    
clean:  
	rm -rf ${OBJSDIR}/*.o  
	rm -rf ${TARGET}  