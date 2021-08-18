#ifndef MPD_DATA_PROCESSOR_H
#define MPD_DATA_PROCESSOR_H

#include <ap_int.h>
#include <hls_stream.h>

#define APV_NUM_MAX     16
#define APV_STRIPS      128
#define APV_SAMPLE_MAX  6

typedef struct
{
  ap_uint<32> data;
  ap_uint<1> end;
} event_data_t;

typedef struct
{
  ap_uint<5> mpdid;
  ap_uint<4> apvid;
  ap_uint<1> evtend;
} header_t;

#define TAG_BLOCK_TRAILER   0x1
#define TAG_APV_HEADER      0x2
#define TAG_APV_TRAILER     0x3
#define TAG_APV_ID_MASK     0x3C

typedef struct
{
  ap_int<13> data;
} sample_data_t;

//from Xinzhan's
typedef ap_int<18> apv_common_mode_sum_t;
typedef ap_int<13> apv_common_mode_avg_t;


void mpd_data_processor_main(
    hls::stream<event_data_t> &s_evIn,
    hls::stream<event_data_t> &s_evOut,
    ap_uint<1> build_all_samples,
    ap_uint<1> build_debug_headers,
    ap_uint<1> enable_cm,
    ap_uint<5> fiber,
    ap_int<26> m_offset[APV_NUM_MAX][APV_STRIPS/2],
    ap_int<13> m_apvThr[APV_NUM_MAX][APV_STRIPS],
    ap_int<13> m_apvThrB[APV_NUM_MAX][APV_STRIPS]
  );

#endif
