############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2017 Xilinx, Inc. All Rights Reserved.
############################################################
set_directive_dataflow "mpd_data_processor_main"
set_directive_data_pack "mpd_data_processor_main" s_evIn
set_directive_interface -mode ap_fifo "mpd_data_processor_main" s_evIn
set_directive_data_pack "mpd_data_processor_main" s_evOut
set_directive_interface -mode ap_fifo "mpd_data_processor_main" s_evOut
set_directive_interface -mode ap_stable "mpd_data_processor_main" build_all_samples
set_directive_interface -mode ap_stable "mpd_data_processor_main" build_debug_headers
set_directive_interface -mode ap_stable "mpd_data_processor_main" enable_cm
set_directive_interface -mode ap_stable "mpd_data_processor_main" fiber
set_directive_resource -core RAM_1P_BRAM "mpd_data_processor_main" m_offset
set_directive_resource -core RAM_1P_BRAM "mpd_data_processor_main" m_apvThr
set_directive_resource -core RAM_1P_BRAM "mpd_data_processor_main" m_apvThrB
set_directive_pipeline -II 2 -enable_flush "frame_decoder"
set_directive_pipeline -II 64 -enable_flush "avgHeaderDiv"
set_directive_pipeline -II 2 -enable_flush "event_writer"
set_directive_interface -mode ap_ctrl_none "mpd_data_processor_main"
set_directive_pipeline -enable_flush "apv_sorting_hls"
set_directive_stream -depth 8 -dim 1 "mpd_data_processor_main" s_header
set_directive_stream -depth 8 -dim 1 "mpd_data_processor_main" s_apv_common_mode_avg
set_directive_stream -depth 8 -dim 1 "mpd_data_processor_main" s_apv_common_mode_sum
