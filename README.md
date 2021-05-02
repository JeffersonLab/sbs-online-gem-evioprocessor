# sbs-online-gem-evioprocessor
Processes EVIO files with SSP/VTP recorded data from GEM and applies hardware common-mode/zero suppression, and rewrites data into separate bank


------------------------
Libraries required
------------------------

ROOT (for analysis code/as needed)
EVIO v5.2:  https://github.com/JeffersonLab/evio/tree/evio-5.2
Vivado HLS (already included in repository)


------------------------
Compiling
------------------------
make

------------------------
Running
------------------------
bin/cmzero <evio_input_filename> <pedestal_input_filename> <common-mode_input_filename>

Results will be written out to a new evio file matching the path & name of the input file with "_out" appended to the name.


------------------------
Compile notes
------------------------
Macros:
PRINT_EVENT_ORIG when set to 1: will print the SSP event data from the evio input file
PRINT_EVE_T_SSP  when set to 1: will print the SSP event data after the software applied common-mode/zero suppresion processing

Event processing:
eventProc() after mpdssp_DecodeEvent(), the ApvEvent_ssp contains the processed APV data (ApvEvent_orig is the original). These apvEvent_t buffers can then be analyzed if desired.

Settings for the SSP processing (threshold_rms_factor, build_all_samples, build_debug_hedaers, enable_cm) are set in eventProc(). There can be mroe than 1 instance of these settings and calls to mpdssp_ProcessEvent() in case more than 1 processed output is desired (e.g. analyzing a variety of thresholds). The new bank number of the processed output is set in the first parameter of evioDOMNode::createEvioDOMNode()

