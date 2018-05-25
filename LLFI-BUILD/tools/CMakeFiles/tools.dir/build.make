# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/gpli/LLFI-src

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/gpli/LLFI-BUILD

# Utility rule file for tools.

# Include the progress variables for this target.
include tools/CMakeFiles/tools.dir/progress.make

tools/CMakeFiles/tools: tools/tracediff
tools/CMakeFiles/tools: tools/traceontograph
tools/CMakeFiles/tools: tools/tracetools.py
tools/CMakeFiles/tools: tools/traceunion
tools/CMakeFiles/tools: tools/compiletoIR

tools/tracediff: /home/gpli/LLFI-src/tools/tracediff.py
	$(CMAKE_COMMAND) -E cmake_progress_report /home/gpli/LLFI-BUILD/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold "Generating tracediff"
	cd /home/gpli/LLFI-BUILD/tools && /usr/bin/cmake -E copy /home/gpli/LLFI-src/tools/tracediff.py /home/gpli/LLFI-BUILD/tools/tracediff

tools/traceontograph: /home/gpli/LLFI-src/tools/traceontograph.py
	$(CMAKE_COMMAND) -E cmake_progress_report /home/gpli/LLFI-BUILD/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold "Generating traceontograph"
	cd /home/gpli/LLFI-BUILD/tools && /usr/bin/cmake -E copy /home/gpli/LLFI-src/tools/traceontograph.py /home/gpli/LLFI-BUILD/tools/traceontograph

tools/tracetools.py: /home/gpli/LLFI-src/tools/tracetools.py
	$(CMAKE_COMMAND) -E cmake_progress_report /home/gpli/LLFI-BUILD/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold "Generating tracetools.py"
	cd /home/gpli/LLFI-BUILD/tools && /usr/bin/cmake -E copy /home/gpli/LLFI-src/tools/tracetools.py /home/gpli/LLFI-BUILD/tools/tracetools.py

tools/traceunion: /home/gpli/LLFI-src/tools/traceunion.py
	$(CMAKE_COMMAND) -E cmake_progress_report /home/gpli/LLFI-BUILD/CMakeFiles $(CMAKE_PROGRESS_4)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold "Generating traceunion"
	cd /home/gpli/LLFI-BUILD/tools && /usr/bin/cmake -E copy /home/gpli/LLFI-src/tools/traceunion.py /home/gpli/LLFI-BUILD/tools/traceunion

tools/compiletoIR: /home/gpli/LLFI-src/tools/compiletoIR.py
	$(CMAKE_COMMAND) -E cmake_progress_report /home/gpli/LLFI-BUILD/CMakeFiles $(CMAKE_PROGRESS_5)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold "Generating compiletoIR"
	cd /home/gpli/LLFI-BUILD/tools && /usr/bin/cmake -E copy /home/gpli/LLFI-src/tools/compiletoIR.py /home/gpli/LLFI-BUILD/tools/compiletoIR

tools: tools/CMakeFiles/tools
tools: tools/tracediff
tools: tools/traceontograph
tools: tools/tracetools.py
tools: tools/traceunion
tools: tools/compiletoIR
tools: tools/CMakeFiles/tools.dir/build.make
.PHONY : tools

# Rule to build all files generated by this target.
tools/CMakeFiles/tools.dir/build: tools
.PHONY : tools/CMakeFiles/tools.dir/build

tools/CMakeFiles/tools.dir/clean:
	cd /home/gpli/LLFI-BUILD/tools && $(CMAKE_COMMAND) -P CMakeFiles/tools.dir/cmake_clean.cmake
.PHONY : tools/CMakeFiles/tools.dir/clean

tools/CMakeFiles/tools.dir/depend:
	cd /home/gpli/LLFI-BUILD && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/gpli/LLFI-src /home/gpli/LLFI-src/tools /home/gpli/LLFI-BUILD /home/gpli/LLFI-BUILD/tools /home/gpli/LLFI-BUILD/tools/CMakeFiles/tools.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tools/CMakeFiles/tools.dir/depend
