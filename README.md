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
bin/cmzero <evio_filename>

