#ifndef MPD_DATA_PROCESSOR_WRAPPER_H
#define MPD_DATA_PROCESSOR_WRAPPER_H

#include <stdint.h>
#include <unistd.h>
#include <vector>
#include "mpd_data_processor.h"

#define FIBER_NUM         32

using namespace std;

// Contains full event
typedef struct
{
  int32_t  data[FIBER_NUM][APV_NUM_MAX][APV_STRIPS][APV_SAMPLE_MAX];
  int32_t  mpdid[FIBER_NUM];
  int32_t  valid[FIBER_NUM]; //bit position indicates valid APVID
  uint32_t event_number;
  uint64_t event_time;
  int32_t  slot; 
} apvEvent_t;


// Raw SSP EVIO data vector -> apvEvent_t
void mpdssp_DecodeEvent(vector<uint32_t> *vec, apvEvent_t *evt);

// Print apvEvent_t
void mpdssp_PrintEvent(apvEvent_t *evt);

// apvEvent_t -> Raw SSP EVIO data vector (processed)
vector<uint32_t> mpdssp_ProcessEvent(
    apvEvent_t *evt,
    float threshold_rms_factor,
    int build_all_samples,
    int build_debug_headers,
    int enable_cm
  );

void LoadCommonMode(const char *filename);

void LoadPedestals(const char *filename);

extern int offset_All[FIBER_NUM][APV_NUM_MAX][APV_STRIPS];
extern int avgAmin_All[FIBER_NUM][APV_NUM_MAX];
extern int avgAmax_All[FIBER_NUM][APV_NUM_MAX];
extern float apvThr_All[FIBER_NUM][APV_NUM_MAX][APV_STRIPS];

#endif
