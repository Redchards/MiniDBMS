# This makefile is geared towards begginers and people who want fast prototyping and/or testing,
# and even if not willing to take the time to write a makefile, wants to have a configurable and yet
# solid solution. It supports natively C, C++ and assembler language, provided your C/C++ compiler can
# assemble assembler language file, but virtually any does. With minor modification, it also
# supports Haskell (we only need to add a flah variable, and a compilation rule for haskell, and setting
# the compiler to ghc for example), but this version do not.
# So yes, this makefile is general purpose, but really only useful for small to average projects.
# Indeed, bigger projects will require build mecanism far more advanced than such a simple makefile
# (or beware of everlasting compilation time ...).
# So, even if this makefile should be more than enough in general, there's few caveheats you should
# be aware of.
# First of all, to save typing and hassle, this makefile do a bunch of things "automagically". This, of course,
# includes some overhead (but usually not that much). So if you want really fast iteration time, this may be not
# the right solution.
# Second, even if I tried to make configuration as easy as possible, some more advanced task (cross compiling) will
# require some tweaks. It's explained in the "HOWTO" below.
# Finally, when you type the make command, some arguments will not be treated as target by the script
# For example, in the command "make clean release", clean will be the target rule, and release will do
# nothing but setting the configuration to release mod. So only the first argument count as a target here.
# But this case is order-sensitive, as doing "make release clean" will first execute the release rule,
# then clean the temporaries produced by this rule. This is a nice side effect, but this can be confusing
# if you don't know this at first.
#
# HOWTO : - you can set the project name by setting the "EXEC" variable to your liking. By default, it is
# the name of the directory the makefile is in.
#		  - the default directory structure required by the makefile is : "src" for the source files and
# "include" for the header files. You can change this by setting "SRCDIR" and "INCLDIR" variables to your
# liking.
#		  - make clean is the only command which is order dependent (in order to keep the side effect
# explained above).
#         - configurations are autogenerated from ALLPLATFORMS and ALLCONFIGS lists. It takes the form of
# config-platform. So, if you want to compile in release mod for 32 bit intel (x86), you should type
# "make release-x86".
#		  - compilation mod is by default debug, and platform x86. You can adjust these values
# by changind DEFAULTCONFIG and DEFAULTPLATFORM variables.
#		  - to compile to llvm bitcode, juste type "make xxx jit" where xxx is the configuration you want.
# same thing for analysis mod.
#		  - use "make cleantmp" to clean all objs and dependencies.
#		  - use "make cleanall" to clean absolutly everything, final files included.
#		  - build-info rule is nice (the one which is giving configuration info at the beggining) is nice,
# but can introduce an unwanted performance overhead. Using the option "noinfo" allows to disable it.
#		  - the flags adopt the following convention : the word is transformed to uppercase, and the word
# "FLAGS" is appended. So, for example, to flags corresponding to the "release" mod is "RELEASEFLAGS".
# If flags are intended to be used for linkin, "LDFLAGS" is appended. The only case we got this situation
# is with library flags, for example, the linking flags corresponding to "shared" mod is "SHAREDLDFLAGS"
#		  - if you add new mods or configurations, remember to add new flags accordingly. For example, if one
# was to add "armv7" platform support, he would change the compiler (CXX) to a cross compiler and create a
# ARMV7FLAGS variable, which would contains the required compiler flags for armvs7 platform. Everything will then
# setup automatically. Maybe I will add later and option to set the compiler accordingly to the wanted platform if
# requested.
#		  - this makefile use automatically defined platform dependent directories. For example, $(SRCDIR)/x86 will
# only be considered for a x86 build, and not for a x64 build. It could be very useful, the simplest example is when
# you have external assembly language file, that inherently depends on the platform you compile for.
#
# WARNING : A possible caveheat is that it's not possible to pass arguments in command line. For example, if you do
# make CXXFLAGS+=-std=c++14, the value will erase the value we set inside the makefile (so configurations will not be
# able to change the variable, rendering them near useless). It would be somewhat trivial to modify this to support this.
# How to do so ? With the "override" directive. First, you need to be aware that all subsequent assignement without override
# to variable first (or at one point) set with "override" statement, will have no effect. The "cleanest" way I could think
# of achieving this behaviour, for some flags for instance, would be :
# override DUMMY_CXXFLAGS=$(CXXFLAGS)
# override CXXFLAGS=$(FLAGS) -std=c++11 $(DUMMY_CXXFLAGS)
# This is working well, except that I don't really have need for this kind of behaviour, and it would add yet a whole
# bunch of boilerplate, for something really rarely used :)
# If you need to modify a variable, you really should do so by modifying them directly inside the makefile. The variables
# are well organized for this particular purpose (all flags are at the beginning)

# NOTE : Basically, the performance penalty incured by this makefile is 0.150s compared to a command line, hand written compilation.
# Provided all the advantages it gives, it is acceptable for small/medium test projects.

# The shell that the make will use to execute shell commands.
# It can be set to any decent shell without any problem.
SHELL:=/bin/bash

# We only define the C++ compiler, and it will take care of compiling both C and C++ files.
# We also set the linker (LD) to the same value, as most of the compilers do it through the same command.
CXX:=clang++
LD:= $(CXX)
JITLD:= llvm-link

# The final executable, or library, name.
EXEC:=$(notdir $(shell pwd))

# Various directory path.
# As nothing is hardcoded, the values can be set to the liking of the user, without breaking something.
LIBDIR:= lib
OBJDIR:= obj
SRCDIR:= src
TESTDIR:= test
INCLDIR:= include .
BINDIR:= bin
SCANDIR:= scan

# Extensions of the different types of file
EXEEXT:=
OBJEXT:=o
DEPEXT:=d
CEXT:=c
CXXEXT:=cxx
STATICLIBEXT:=a
SHAREDLIBEXT:=so
ASMEXT:=asm

TESTFRAMEWORK=mettle

# Basic C and C++ flags. Assembler code don't really need flags
FLAGS:= -W -Wall -Wextra
CFLAGS= $(FLAGS) -std=c11
#$(error cxxflags are $(D) and flags are $(FLAGS))
CXXFLAGS= $(FLAGS) -std=c++1z

# Flags used only for debug mod
DEBUGFLAGS:= -g -O0 $(DEBUGFLAGS)

# Flags used only for release mod
RELEASEFLAGS:= -O3 $(RELEASEFLAGS)

# Flags used only for analyzis mod
ANALYSISFLAGS:= --analyze -Xanalyzer -analyzer-output=html -o $(SCANDIR)

# Flags used for different platforms
X86FLAGS:= -m32
X64FLAGS:= -m64

# Flags used to create dependencies
DEPENDFLAGS:= -MMD

# Flags used by the linker
LDFLAGS:=

# Flags used only for bitcode compilation (by LLVM/clang)
JITFLAGS:= -emit-llvm -S -fno-use-cxa-atexit

# Flags to use for different library mod.
STATICLDFLAGS:= rcs
SHAREDFLAGS:= -fPIC
SHAREDLDFLAGS:= -shared

# Flags to add verbose linker and compiler output
# These are the only flags which do not follow the convention explained at the beggining
VERBOSEFLAGS:= -v
VERBOSELDFLAGS:= -v

# Some helper functions
define to_lower
$(strip $(shell echo $1 | tr '[:upper:]' '[:lower:]'))
endef

define to_upper
$(strip $(shell echo $1 | tr '[:lower:]' '[:upper:]'))
endef

# Returns the flags following the convention explained at the beggining
define get_flags
$(strip $($(call to_upper, $1)FLAGS))
endef

# Returns the linking flags following the convention explained at the beggining
define get_ldflags
$(strip $($(call to_upper, $1)LDFLAGS))
endef

# This variable will determine whether we will use native or JIT execution.
# JIT execution will produce llvm bitcode, which can be executed by the llvm execution engine.
# The command "lli" is the simplest way to run them.
# The values are either "native" or "jit".
# The value "jit" is only valid for the clang compiler.
override ALLEXECUTIONS:=native jit
override DEFAULTEXECUTION:=native
override PASSEDEXECUTION:=$(filter $(MAKECMDGOALS), $(ALLEXECUTIONS))
ifeq ($(words $(PASSEDEXECUTION)), 0)
	EXECUTION:=$(DEFAULTEXECUTION)
else ifeq ($(words $(PASSEDEXECUTION)), 1)
	EXECUTION:=$(PASSEDEXECUTION)
else
$(error Error : multiple arguments or invalid execution mod !)
endif

FLAGS+=$(call get_flags, $(EXECUTION))

# Defining supported platforms.
# Of course, more can be added
override ALLPLATFORMS:=x86 x64 x67
ifeq ($(shell getconf LONG_BIT), 64)
override DEFAULTPLATFORM:=x64
else ifeq ($(shell getconf LONG_BIT), 32)
override DEFAULTPLATFORM:=x86
endif

# List of all available compilation mods.
override ALLCONFIGS:=debug release analysis
# The default config.
override DEFAULTCONFIG:=debug
# Build the configuration names, following the convention explained at the beggining.
override CONFIG_PLATFORM:=$(foreach CONFIG, $(filter-out analysis, $(ALLCONFIGS)),$(addprefix $(CONFIG)-, $(ALLPLATFORMS)))
# Append compilation mods list to configuration names, to allow used to not specify the platform (choosing the default).
override CONFIG_PLATFORM+=$(ALLCONFIGS)

# Parse the user input, and extract build mod and platform.
override PASSEDCONFIG:=$(filter $(MAKECMDGOALS), $(CONFIG_PLATFORM))
ifeq ($(words $(PASSEDCONFIG)), 0)
	CONFIG:=$(DEFAULTCONFIG)
	PLATFORM:=$(DEFAULTPLATFORM)
else ifeq ($(words $(PASSEDCONFIG)), 1)
	CONFIG:=$(filter $(foreach PLATFORM, $(ALLPLATFORMS),$(patsubst %-$(PLATFORM),%,$(PASSEDCONFIG))), $(ALLCONFIGS))
	PLATFORM:=$(filter $(foreach PLATFORM, $(ALLPLATFORMS),$(patsubst %-$(PLATFORM),$(PLATFORM),$(PASSEDCONFIG))), $(ALLPLATFORMS))
else
$(error Error : multiple configuration or invalid configuration mod !)
endif
# If no platform was defined, and the passed mod wasn't analysis, we set the platform to the default value.
ifeq ($(PLATFORM),)
ifneq ($(CONFIG), analysis)
	PLATFORM:=$(DEFAULTPLATFORM)
endif
endif

FLAGS+=$(call get_flags, $(CONFIG))
#$(error $(CXXFLAGS))
private TMP:=$(call get_flags, $(PLATFORM))
FLAGS+=$(TMP)
# If the compilation mod is not JIT, then the linker also need platform flags infos.
ifneq ($(EXECUTION), jit)
	LDFLAGS+=$(TMP)
endif

# If we got only a trailing dash (compilation mod is analysis), remove it and only assign the compilaion mod.
# Else, build congname accordingly.
override CONFIGNAME:=$(patsubst %-, %, $(CONFIG)-$(PLATFORM))

# Set options for execution mods.
# Native execution mod do not change anything.
ifeq ($(EXECUTION), jit)
ifeq ($(CONFIG), analysis)
$(warning Warning : analysis mod is independant of execution mod. No bitcode will be generated)
endif
ifeq (, $(filter $(CXX), clang clang++))
$(error Error : attempt to emit llvm IR, but not using LLVM/clang)
endif
	LD= $(JITLD)
	EXEC:= $(addprefix $(EXEC).,bc)
	OBJEXT:= bco
endif
ifeq ($(CONFIG), analysis)
ifeq (, $(filter $(CXX), clang clang++))
$(error Error : attempt to enable analysis, but not using LLVM/clang)
endif
endif

# Parse library configuration, and set options accordingly.
private override LIBTYPES:=static shared
private override PASSEDLIBTYPE:=$(filter $(MAKECMDGOALS), $(LIBTYPES))
ifneq ($(PASSEDLIBTYPE),)
ifeq ($(EXECUTION), jit)
$(error : Can't build bitcode library ! Please select native execution mode (default))
endif
ifneq ($(words $(PASSEDLIBTYPE)), 1)
$(error : Can't build multiple type of lib at the same time. Please select only one !)
else ifeq ($(PASSEDLIBTYPE), static)
# Static libs are really just archives of objects, so no platform information is needed
# (it's already contained in the objects)
	LDFLAGS:=$(filter-out $(call get_flags, $(PLATFORM)), $(LDFLAGS))$(call get_ldflags, static)
	LD:=ar
	EXEC:=lib$(call to_lower, $(EXEC)).$(STATICLIBEXT)
else ifeq ($(PASSEDLIBTYPE), shared)
	FLAGS+=$(call get_flags, shared)
	LDFLAGS+=$(call get_ldflags, shared)
	EXEC:=lib$(call to_lower, $(EXEC)).$(SHAREDLIBEXT)
# Change objs extension because we can't use .o files produced by default rules to produce a shared library,
# and we don't want to clean object if we build shared lib after static lib.
# Dynamic linking requires dynamic reallocation (-fPIC flag for gcc and clang).
	OBJEXT:=osh
endif
# If the exe extension is not empty (on windows for example), add it to the exe name.
else ifneq ($(EXEEXT),)
	EXEC:=$(EXEC).$(EXEEXT)
endif

# Character used to make the makefile silent.
SILENT:=@

# Turns on/off the verbose output of the makefile.
# The values are either "none", "some", "detail" or "all"
# Verbose should turn verbose options for compiler and linker too !
VERBOSE:=none

ifneq ($(filter $(VERBOSE), some detail all),)
ifneq ($(filter $(VERBOSE), detail all),)
	FLAGS+=$(VERBOSEFLAGS)
	LDFLAGS+=$(VERBOSELDFLAGS)
ifeq ($(VERBOSE), all)
	SHELL_:=$(SHELL)
	SHELL=$(warning [$@])$(SHELL_) -x
endif
endif
	SILENT:=
endif


# Creation of sources, objects and dependencies list.
# NOTE : Can't we just do "subst" src/ by obj/$PLATFORM/$CONFIG in the first place, thus eliminating
# the need for the addprefix ?
# The code is a bit complex due to the fact that the makefile allows to have platform specific directories, we
# thus need to discard other platform specific directories. If we had, for instance, x86, x64 and ARM, and the current
# configuration was x86, we would discard src/x64 and src/ARM from our search path. As for performance, it's almost
# invisible.
# Of course, it is possible to remove all intermediate variables, but it would be even more unreadable.
override EXCLUDEDIRS:=$(filter-out $(PLATFORM), $(ALLPLATFORMS))
override LASTEXCLUDEDIR:=$(lastword $(EXCLUDEDIRS))
override EXCLUDEDIR_HEAD:=$(filter-out $(LASTEXCLUDEDIR), $(EXCLUDEDIRS))
override EXCLUDEDIR_ARGS:=$(patsubst  %, -path $(SRCDIR)/% -o, $(EXCLUDEDIR_HEAD))$(patsubst %, -path $(SRCDIR)/%, $(lastword $(EXCLUDEDIRS)))
override GREP_ARGS:=$(patsubst %, %\|, $(EXCLUDEDIR_HEAD))$(lastword $(EXCLUDEDIRS))

SRC:=$(shell find $(SRCDIR) \( $(EXCLUDEDIR_ARGS) \) -prune -o -type f -name '*.$(CEXT)' -o -name '*.$(CXXEXT)' -o -name '*.$(ASMEXT)' | grep -v "$(SRCDIR)/\($(strip $(GREP_ARGS))\)")

OBJ:=$(subst $(SRCDIR)/,,$(SRC:.$(CEXT)=.$(OBJEXT)))
OBJ:=$(subst $(SRCDIR)/,,$(OBJ:.$(CXXEXT)=.$(OBJEXT)))
OBJ:=$(subst $(SRCDIR)/,,$(OBJ:.$(ASMEXT)=.$(OBJEXT)))
OBJS:=$(addprefix $(OBJDIR)/$(PLATFORM)/$(CONFIG)/, $(OBJ))
DEPS:=$(OBJ:.$(OBJEXT)=.$(DEPEXT))


# Are we in test mod ?
ifeq ($(firstword $(MAKECMDGOALS)),test)
	INCLDIR+= $(TESTDIR)/$(TESTFRAMEWORK)/include

	SRC:=$(shell find $(TESTDIR) -type f -name '*.$(CEXT)' -o -name '*.$(CXXEXT)' -o -name '*.$(ASMEXT)')

	TEST:=$(subst $(SRCDIR)/,,$(SRC:.$(CEXT)=))
	TEST:=$(subst $(SRCDIR)/,,$(TEST:.$(CXXEXT)=))
	TESTDEPS:=$(subst $(SRCDIR)/,,$(SRC:.$(CEXT)=.$(DEPEXT)))
	TESTDEPS:=$(subst $(SRCDIR)/,,$(TESTDEPS:.$(CXXEXT)=.$(DEPEXT)))
	#TESTOBJ:=$(subst $(SRCDIR)/,,$(TESTOBJ:.$(ASMEXT)=.$(OBJEXT)))
	TESTS=$(addprefix $(BINDIR)/$(PLATFORM)/$(CONFIG)/, $(TEST))
	override TESTMOD=test
	DEPS+=$(TESTDEPS)
$(warning $(DEPS))
	#TEST_OBJS=$(addprefix $(OBJDIR)/$(TESTDIR)/$(PLATFORM)/$(CONFIG)/, $(OBJ))
else ifneq ($(filter $(MAKECMDGOALS),test),)
$(warning "The 'test' option will not be taken in account unless in first position.");
endif

# Define the path where the result will be outputted
OUTPATH:=$(if $(filter $(CONFIG), analysis),$(SCANDIR),$(BINDIR)/$(PLATFORM)/$(CONFIG))

# These flag variables will not change depending on the FLAGS variable anymore, so we set them to immediate variables
# to avoid expanding them each time we need them (possibly a lot, one time by compilation unit)
# NOTE : If you need to modify CFLAGS and CXXFLAGS, modify the version at the top of this makefile !
CFLAGS:=$(CFLAGS)
CXXFLAGS:=$(CXXFLAGS)

# .PHONY targets.
.PHONY: test clean cleantmp cleanall $(CONFIG_PLATFORM) $(ALLEXECUTIONS)

# .PRECIOUS objects.
.PRECIOUS: %.(CXXEXT) %.(CEXT) %.(ASMEXT) %.(OBJEXT)


# .SILENT: .SECONDARY

# Rule "all". All other first rules depends on it.
all: build-info $(OUTPATH)/$(EXEC)
	@$(if $(OK),,\
	printf "\e[1m\e[32mNothing to do, everything is up to date !\e[0m\n\n")

#test: $(OUTPATH)/$(TESTDIR)/$(EXEC)
#	@ printf $INCLDIR

test: build-info $(OBJS) $(TESTS)
	@$(if $(OK),printf "Built : $(addprefix - \e[1m\e[32m,$(addsuffix \n,$(notdir $(filter $?, $(TESTS)))))\e[0mSee the result in the following directory : \e[1m\e[96m$(OUTPATH)/$(TESTDIR)\e[0m\n",\
				printf "\e[1m\e[32mNothing to do, everything is up to date !\e[0m\n\n")

# If the first option is not clean, we call the "all" rule.
ifeq ($(filter $(firstword $(MAKECMDGOALS)), clean),)
$(CONFIG_PLATFORM): all
else
$(CONFIG_PLATFORM):
	@ echo "Done !"
endif

# Foo rules for options.
# Using the "@#" to make a "silent comment". Yhea, strange enough I figured that myself ...

# If the first option is not an execution, we don't do anything.
# Else, we initiate the build.
ifeq ($(filter $(firstword $(MAKECMDGOALS)), $(ALLEXECUTIONS)),)
$(ALLEXECUTIONS):
	@#
else
$(ALLEXECUTIONS): $(CONFIGNAME)
endif

# If the first otion is not a library type, we don't do anything.
# Else, we initiate the build.
ifeq ($(filter $(firstword $(MAKECMDGOALS)), $(LIBTYPES)),)
$(LIBTYPES):
	@#
else
$(LIBTYPES): $(CONFIGNAME)
endif

# Do we want build-info to run ?
ifeq ($(firstword $(MAKECMDGOALS)), noinfo)
noinfo : $(CONFIGNAME)
else
noinfo:
	@#
endif

-include $(addprefix obj/$(PLATFORM)/$(CONFIG)/,$(DEPS))

# This is the main rule.
# It will link all file generated by its dependencies together.
$(OUTPATH)/$(EXEC) : $(OBJS)
	$(eval OK=1)
	$(SILENT) mkdir -p $(OUTPATH)
ifeq ($(EXECUTION), jit)
	$(SILENT) $(LD) $(LDFLAGS) $^ -o $(OUTPATH)/$(EXEC)
else
	$(if $(filter $(CONFIG), analysis),\
	@ printf "No executable code output for analysis",\
	$(SILENT) printf "Linking binary ... ";\
	$(LD) $(LDFLAGS) -o $(OUTPATH)/$(EXEC) $^;\
	printf "Done !\n")
endif
	@ printf "\e[1m\e[32mGeneration successful !\e[0m\n"
ifeq ($(EXECUTION), native)
	$(SILENT) $(if $(filter $(CONFIG), release),\
	printf "Stripping binary ... ";\
	strip $(OUTPATH)/$(EXEC);\
	echo "Done !",)
else
	@ printf "\e[1m\e[92mBitcode generated\e[0m\n"
endif
	@ printf "Resulting file : \e[1m\e[92m$(EXEC)\e[0m\n\
	See the result in the following directory : \e[1m\e[96m$(OUTPATH)\e[0m\n"

$(BINDIR)/$(PLATFORM)/$(CONFIG)/$(TESTDIR)/%: $(OBJDIR)/$(PLATFORM)/$(CONFIG)/$(TESTDIR)/%.$(OBJEXT)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(LD) $(LDFLAGS) $^ -o $@

# Eval might be a bottleneck here for larger projects
# Generation of obj file for C source
$(OBJDIR)/$(PLATFORM)/$(CONFIG)/%.$(OBJEXT): $(SRCDIR)/%.$(CEXT)
	$(eval OK=1)
	$(SILENT) mkdir -p $(@D)
	$(error $(@D))
	$(SILENT) $(CXX) $(addprefix -I, $(INCLDIR)) -x c -o $@ -c $< $(CFLAGS) $(DEPENDFLAGS)

# Generation of obj file for C++ source
$(OBJDIR)/$(PLATFORM)/$(CONFIG)/%.$(OBJEXT): $(SRCDIR)/%.$(CXXEXT)
	$(eval OK=1)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(CXX) $(addprefix -I, $(INCLDIR)) -x c++ -o $@ -c $< $(CXXFLAGS) $(DEPENDFLAGS)

# Generationr of obj file for assembly
$(OBJDIR)/$(PLATFORM)/$(CONFIG)/%.$(OBJEXT): $(SRCDIR)/%.$(ASMEXT)
	$(eval OK=1)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(CXX) -x assembler -o $@ -c $< $(FLAGS)

# Generation of test obj file for C source
$(OBJDIR)/$(PLATFORM)/$(CONFIG)/$(TESTDIR)/%.$(OBJEXT): $(TESTDIR)/%.$(CEXT)
	$(eval OK=1)
	$(SILENT) mkdir -p $(@D)
	@ printf "Compiling test : \e[1m\e[92m$*\e[0m\n"
	@(SILENT) $(CXX) $(CFLAGS) -x c $(addprefix -I, $(INCLDIR)) $(DEPENDFLAGS) -c $< -o $@ 

# Generation of test obj file for C++ source
$(OBJDIR)/$(PLATFORM)/$(CONFIG)/$(TESTDIR)/%.$(OBJEXT): $(TESTDIR)/%.$(CXXEXT)
	$(eval OK=1)
	$(SILENT) mkdir -p $(@D)
	@ printf "Compiling test : \e[1m\e[92m$*\e[0m\n"
	$(SILENT) $(CXX) $(CXXFLAGS) -x c++ $(addprefix -I, $(INCLDIR)) $(DEPENDFLAGS) -c $< -o $@
# We want the following rule to run in a sequential way (we don't want the building and the cleaning mixed together)
.NOTPARALLEL:

# The clean rule will conditionally delete temporary objects.
# For example, if you type "make clean debug", it will clean only debug native objects (not bitcodes ones).
# The same effect can be achieve by typing "make clean", as debug is the default configuration,
# in the original makefile at least
clean:
	@echo "Cleaning $(CONFIG) $(EXECUTION) temporary objects ..."
	$(SILENT) find $(OBJDIR)/$(PLATFORM)/$(CONFIG) -name "*.$(OBJEXT)" -type f -delete -o -name "*.$(DEPEXT)" -type f -delete

# Clean every tmp files produced by different compilations.
cleantmp:
	$(SILENT) rm -rf $(OBJDIR)/*

# Clean absolutly everything, including produced binaries.
cleanall: cleantmp
	$(SILENT) rm -f ./build.gen
	$(SILENT) rm -rf $(BINDIR)/*


# The summary of the upcoming compilation configuration printed at the begining.
# It could be extended, but it is sufficient like this.
# If can be disabled by calling the make using "noinfo".
HEADER_MSG:="COMPILATION OF PROJECT : $(EXEC)"
LINE:=$(shell printf "%$$(tput cols)s" | tr " " "=")
ifeq ($(filter $(MAKECMDGOALS), noinfo),)
build-info:
	@printf $(LINE)"\n\e[1m\e[91m$(shell export HEADER_MSG=$(HEADER_MSG); printf " %.0s" $$(seq 1 $$(($$(tput cols)/2 - $${#HEADER_MSG}/2)))) "$(HEADER_MSG)"\e[0m\n\
	Generation configuration is \e[1m\e[32m$(EXECUTION) $(if$(TESTMOD),$(TESTMOD),)$(CONFIGNAME)\e[0m\n\n\
	Sources directory search path is :   \e[1m\e[96m$(SRCDIR)\e[0m (SRCDIR)\n\
	Includes directory search path is :  \e[1m\e[96m$(INCLDIR)\e[0m (INCLDIR)\n\
	Libraries directory search path is : \e[1m\e[96m$(LIBDIR)\e[0m (LIBDIR)\n\
	Objects directory is :               \e[1m\e[96m$(OBJDIR)\e[0m (OBJDIR)\n\
	The binary output path is :          \e[1m\e[96m$(OUTPATH)/$(trim$(if $(filter $(TESTMOD),test), $(TESTMOD),))\e[0m\n\n\
	$(LINE)\n\n"
else
build-info:
endif
