# Makefile

# Directory names
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Input Names
CPP_SRC_PATHS = $(wildcard $(SRCDIR)/*.cpp)
CPP_FILES = $(CPP_SRC_PATHS:$(SRCDIR)/%=%)
CPP_FILES := $(filter-out simulation.cpp,$(CPP_FILES))

# -----------------------------------------------------------------------------

# C++ Compiler and Flags
BOOST_PATH = /central/software/boost/1_66_0
BOOST_INC_PATH = $(BOOST_PATH)/include
BOOST_LIB_PATH = $(BOOST_PATH)/lib

GPP = g++
FLAGS = -g -Wall -std=c++11 -pthread -O0
INCLUDE = -I$(BOOST_INC_PATH)

# -----------------------------------------------------------------------------
# Object files
# -----------------------------------------------------------------------------

# C++ Object Files
CPP_OBJ = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(CPP_FILES)))

# -----------------------------------------------------------------------------
# Make rules
# -----------------------------------------------------------------------------

LINK = $(GPP) $(FLAGS) -o $(BINDIR)/$@ $(INCLUDE) $^

# Top level rules

all: ga_simulation_2d ga_simulation_3d

ga_simulation_2d: DIMENSIONS = 2
ga_simulation_2d: $(CPP_OBJ) $(OBJDIR)/simulation_2d.cpp.o
	$(LINK)

ga_simulation_3d: DIMENSIONS = 3
ga_simulation_3d: $(CPP_OBJ) $(OBJDIR)/simulation_3d.cpp.o
	$(LINK)

# Compile C++ Source Files
$(CPP_OBJ): $(OBJDIR)/%.o : $(SRCDIR)/%
	$(GPP) $(FLAGS) -c -o $@ $(INCLUDE) $<

# Custom rule to define dimension
$(OBJDIR)/simulation_%.cpp.o: $(SRCDIR)/simulation.cpp
	$(GPP) $(FLAGS) -DDIMENSIONS=$(DIMENSIONS) -c -o $@ $(INCLUDE) $<

# Clean everything including temporary Emacs files
clean:
	rm -f $(BINDIR)/* $(OBJDIR)/*.o $(SRCDIR)/*~ *~

.PHONY: clean all
