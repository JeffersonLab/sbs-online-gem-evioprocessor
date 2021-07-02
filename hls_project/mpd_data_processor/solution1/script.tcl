############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2017 Xilinx, Inc. All Rights Reserved.
############################################################
open_project mpd_data_processor
set_top mpd_data_processor_main
add_files ../mpd_data_processor.cpp
add_files ../mpd_data_processor.h
add_files -tb ../mpd_data_processor_tb.cpp
add_files -tb ../mpd_data_processor_wrapper.cpp
add_files -tb ../mpd_data_processor_wrapper.h
open_solution "solution1"
set_part {xc7a100tcsg324-1}
create_clock -period 8 -name default
set_clock_uncertainty 1
source "./mpd_data_processor/solution1/directives.tcl"
csim_design -compiler gcc
csynth_design
cosim_design -compiler gcc -trace_level all -rtl vhdl -tool xsim
export_design -format ip_catalog
