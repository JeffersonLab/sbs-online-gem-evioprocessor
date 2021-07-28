#include <hls_stream.h>
#include <ap_int.h>
#include <iostream>
#include "mpd_data_processor.h"

#define BLOCK_HEADER    0x10
#define BLOCK_TRAILER   0x1
#define EVENT_TRAILER   0x5

#define APV_CH_DATA     0x14

#define APV_HEADER      0x0
#define APV_DATA        0x1
#define APV_TRAILER     0x2

void frame_decoder(
    hls::stream<event_data_t> &s_evIn,
    hls::stream<sample_data_t> &s_sample_data,
    ap_int<26> m_offset[APV_NUM_MAX][APV_STRIPS/2],
    hls::stream<avg_pre_header_t> &s_avgAPreHeader
  )
{
  static enum eState {S_BLOCK_HEADER, S_APV_HEADER, S_APV_DATA, S_APV_TRAILER, S_ERROR} ps = S_BLOCK_HEADER;
  static ap_uint<5> mpd_id;
  static ap_uint<6> adc_word_cnt;
  static ap_uint<4> apv_id;
  static min_max_t minmax;
  static avg_pre_header_t avg_pre_header;
  sample_data_t sample_data;
  event_data_t val;
  ap_uint<3> header;

  switch(ps)
  {
    case S_BLOCK_HEADER:
      if(!s_evIn.empty())
      {
        val = s_evIn.read();
        if(val.data(31,27) == BLOCK_HEADER)
        {
          mpd_id = val.data(20,16);
          ps = S_APV_HEADER;
        }
      }
      break;

    case S_APV_HEADER:
      if(!s_evIn.empty())
      {
        val = s_evIn.read();
        if(val.end)
        {
          avg_pre_header.sum(1,0) = AVG_HEADER_TYPE_EVEND;
          avg_pre_header.sum(19,2) = 0;
          avg_pre_header.tag = 1;
          s_avgAPreHeader.write(avg_pre_header);
          ps = S_BLOCK_HEADER;
        }
        else if(val.data(31,27) == APV_CH_DATA)
        {
          apv_id = val.data(26,23);

          avg_pre_header.sum(1,0) = AVG_HEADER_TYPE_APVHDR;
          avg_pre_header.sum(5,2) = apv_id;
          avg_pre_header.sum(10,6) = mpd_id;
          avg_pre_header.tag = 1;
          s_avgAPreHeader.write(avg_pre_header);

          adc_word_cnt = 0;

          avg_pre_header.sum = 0;
          avg_pre_header.cnt = 0;
          avg_pre_header.tag = 0;

          ap_uint<26> offset_data = m_offset[15][apv_id];
          minmax.min(12,0) = offset_data(12,0);
          minmax.max(12,0) = offset_data(25,13);
          ps = S_APV_DATA;
        }
      }
      break;

    case S_APV_DATA:
      if(!s_evIn.empty())
      {
        ap_fixed<13,13,AP_RND,AP_SAT> adc;
        ap_uint<26> offset_data = m_offset[apv_id][adc_word_cnt];

        val = s_evIn.read();

        //Sample0
        adc = (ap_int<13>)val.data(12,0) - (ap_int<13>)offset_data(12,0);
        sample_data.data = adc;
        s_sample_data.write(sample_data);
        if((adc >= minmax.min) && (adc <= minmax.max))
        {
          avg_pre_header.sum += adc;
          avg_pre_header.cnt++;
        }

        //Sample1
        adc = (ap_int<13>)val.data(25,13) - (ap_int<13>)offset_data(25,13);
        sample_data.data = adc;
        s_sample_data.write(sample_data);
        if((adc >= minmax.min) && (adc <= minmax.max))
        {
          avg_pre_header.sum += adc;
          avg_pre_header.cnt++;
        }

        if(adc_word_cnt == 63)
          ps = S_APV_TRAILER;

        adc_word_cnt++;
      }
      break;

    case S_APV_TRAILER:
      s_avgAPreHeader.write(avg_pre_header);
      ps = S_APV_HEADER;
      break;

    case S_ERROR:
      break;
  }
}

void avgB(
    hls::stream<sample_data_t> &s_avgASamples,
    hls::stream<avg_header_t> &s_avgAHeader,
    hls::stream<sample_data_t> &s_avgBSamplesOut,
    hls::stream<avg_pre_header_t> &s_avgBPreHeader,
    ap_int<13> m_apvThrB[APV_NUM_MAX][APV_STRIPS]
  )
{
  static enum eState {S_IDLE=1, S_READ_AVGHDR, S_READ_APV_DATA, S_WRITE_AVGHDR, S_ERROR} ps = S_IDLE;
  static ap_uint<4> apv_id;
  static ap_uint<5> mpd_id;
  static ap_int<13> avg;
  static avg_pre_header_t avg_pre_header;
  static ap_uint<8> n;

  switch(ps)
  {
    case S_IDLE:
      if(!s_avgAHeader.empty())
      {
        avg_header_t avg_header = s_avgAHeader.read();
        if(avg_header.tag && (avg_header.avg(1,0)==AVG_HEADER_TYPE_APVHDR))
        {
          apv_id = avg_header.avg(5,2);
          mpd_id = avg_header.avg(10,6);

          avg_pre_header.sum(1,0) = AVG_HEADER_TYPE_APVHDR;
          avg_pre_header.sum(5,2) = apv_id;
          avg_pre_header.sum(10,6) = mpd_id;
          avg_pre_header.tag = 1;
          s_avgBPreHeader.write(avg_pre_header);

          avg_pre_header.sum = 0;
          avg_pre_header.cnt = 0;
          avg_pre_header.tag = 0;

          n = 0;

          ps = S_READ_AVGHDR;
        }
        else if(avg_header.tag && (avg_header.avg(1,0)==AVG_HEADER_TYPE_EVEND))
        {
          avg_pre_header.sum(1,0) = AVG_HEADER_TYPE_EVEND;
          avg_pre_header.tag = 1;
          s_avgBPreHeader.write(avg_pre_header);
        }
        else
          ps = S_ERROR;
      }
      break;

    case S_READ_AVGHDR:
      if(!s_avgAHeader.empty())
      {
        avg_header_t avg_header = s_avgAHeader.read();
        if(avg_header.tag)
          ps = S_ERROR;
        else
        {
          avg = avg_header.avg;
          ps = S_READ_APV_DATA;
        }
      }
      break;

    case S_READ_APV_DATA:
      if(!s_avgASamples.empty())
      {
        sample_data_t sample_data = s_avgASamples.read();
        s_avgBSamplesOut.write(sample_data);

        ap_fixed<13,13,AP_RND,AP_SAT> thr = m_apvThrB[apv_id][n]+avg;
        if(sample_data.data <= thr)
        {
          avg_pre_header.sum += sample_data.data;
          avg_pre_header.cnt++;
        }

        if(n++ == 127)
          ps = S_WRITE_AVGHDR;
      }
      break;

    case S_WRITE_AVGHDR:
      s_avgBPreHeader.write(avg_pre_header);
      ps = S_IDLE;
      break;

    case S_ERROR:
      break;
  }
}

void avgHeaderDiv(
    hls::stream<avg_pre_header_t> &s_avgAPreHeader,
    hls::stream<avg_pre_header_t> &s_avgBPreHeader,
    hls::stream<avg_header_t> &s_avgAHeader,
    hls::stream<avg_header_t> &s_avgBHeader
  )
{
  static enum eLast {A,B} last = A;
  avg_pre_header_t avg_pre_header;
  avg_header_t avg_header;
  bool avg_pre_header_valid = false;

  if( (!s_avgAPreHeader.empty() && !s_avgBPreHeader.empty() && (last==A)) ||
      ( s_avgAPreHeader.empty() && !s_avgBPreHeader.empty()) )
  {
    avg_pre_header = s_avgBPreHeader.read();
    avg_pre_header_valid = true;
    last = B;
  }
  else if( (!s_avgAPreHeader.empty() && !s_avgBPreHeader.empty() && (last==B)) ||
           (!s_avgAPreHeader.empty() &&  s_avgBPreHeader.empty()) )
  {
    avg_pre_header = s_avgAPreHeader.read();
    avg_pre_header_valid = true;
    last = A;
  }

  if(avg_pre_header_valid)
  {
    if(avg_pre_header.tag)
    {
      avg_header.avg = avg_pre_header.sum(12,0);
      avg_header.tag = 1;
    }
    else
    {
      avg_header.avg = (avg_pre_header.cnt > 0) ? (avg_pre_header.sum / avg_pre_header.cnt) : (ap_int<20>)0;
      avg_header.tag = 0;
    }
    if(last==A)
      s_avgAHeader.write(avg_header);
    else
      s_avgBHeader.write(avg_header);
  }
}

void event_writer(
    hls::stream<event_data_t> &s_evOut,
    hls::stream<avg_header_t> &s_avgBHeader,
    hls::stream<sample_data_pair_t> &s_avgBSamplesIn,
    ap_uint<1> build_all_samples,
    ap_uint<1> enable_cm,
    ap_uint<5> fiber,
    ap_int<13> m_apvThr[APV_NUM_MAX][APV_STRIPS]
  )
{
  static enum eState {S_HEADER, S_READ0, S_READ1, S_READ2, S_WRITE0, S_WRITE1, S_WRITE2} ps = S_HEADER;
  static ap_uint<4> apv_id;
  static ap_int<13> avgB[6];
  static ap_fixed<13,13,AP_RND,AP_SAT> s[6];
  static ap_uint<7> sample_n;
  static ap_uint<3> cnt = 0;
  static ap_int<16> sum;
  ap_uint<5> mpd_id;
  event_data_t event_data;

  switch(ps)
  {
    case S_HEADER:
      if(!s_avgBHeader.empty())
      {
        avg_header_t avg_header = s_avgBHeader.read();
        if(avg_header.tag && (avg_header.avg(1,0)==AVG_HEADER_TYPE_APVHDR))
        {
          apv_id = avg_header.avg(5,2);
          mpd_id = avg_header.avg(10,6);
          if(!cnt)
          {
            event_data.data(31,27) = 0x15;
            event_data.data(26,21) = 0;
            event_data.data(20,16) = fiber;
            event_data.data(15,5)  = 0;
            event_data.data(4,0)   = mpd_id;
            event_data.end = 0;
            s_evOut.write(event_data);
          }
        }
        else if(avg_header.tag && (avg_header.avg(1,0)==AVG_HEADER_TYPE_EVEND))
        {
          event_data.data = 0xFFFFFFFF;
          event_data.end = 1;
          s_evOut.write(event_data);
        }
        else if(!avg_header.tag)
        {
          avgB[cnt++] = avg_header.avg;
          if(cnt == 6)
          {
            sample_n = 0;
            cnt = 0;
            ps = S_READ0;
          }
        }
      }
      break;

    case S_READ0:
      if(!s_avgBSamplesIn.empty())
      {
        sample_data_pair_t sample_data_pair = s_avgBSamplesIn.read();
        s[0] = enable_cm ? (ap_fixed<13,13,AP_RND,AP_SAT>)(sample_data_pair.data[0] - avgB[0]) : (ap_fixed<13,13,AP_RND,AP_SAT>)sample_data_pair.data[0];
        s[1] = enable_cm ? (ap_fixed<13,13,AP_RND,AP_SAT>)(sample_data_pair.data[1] - avgB[1]) : (ap_fixed<13,13,AP_RND,AP_SAT>)sample_data_pair.data[1];
        sum  = s[0] + s[1];
        ps = S_READ1;
      }
      break;

    case S_READ1:
      if(!s_avgBSamplesIn.empty())
      {
        sample_data_pair_t sample_data_pair = s_avgBSamplesIn.read();
        s[2] = enable_cm ? (ap_fixed<13,13,AP_RND,AP_SAT>)(sample_data_pair.data[0] - avgB[2]) : (ap_fixed<13,13,AP_RND,AP_SAT>)sample_data_pair.data[0];
        s[3] = enable_cm ? (ap_fixed<13,13,AP_RND,AP_SAT>)(sample_data_pair.data[1] - avgB[3]) : (ap_fixed<13,13,AP_RND,AP_SAT>)sample_data_pair.data[1];
        sum += s[2] + s[3];
        ps = S_READ2;
      }
      break;

    case S_READ2:
      if(!s_avgBSamplesIn.empty())
      {
        sample_data_pair_t sample_data_pair = s_avgBSamplesIn.read();
        s[4] = enable_cm ? (ap_fixed<13,13,AP_RND,AP_SAT>)(sample_data_pair.data[0] - avgB[4]) : (ap_fixed<13,13,AP_RND,AP_SAT>)sample_data_pair.data[0];
        s[5] = enable_cm ? (ap_fixed<13,13,AP_RND,AP_SAT>)(sample_data_pair.data[1] - avgB[5]) : (ap_fixed<13,13,AP_RND,AP_SAT>)sample_data_pair.data[1];
        sum += s[4] + s[5];

        ap_uint<5> max_pos0_test, max_pos5_test;
        ap_uint<16> sum_thr;

        for(int i=0;i<5;i++)
        {
          max_pos0_test[i] = (s[0] > s[1+i]) ? 1 : 0;
          max_pos5_test[i] = (s[5] > s[i]  ) ? 1 : 0;
        }

        sum_thr = 6 * m_apvThr[apv_id][sample_n];

        // Check all s[].apv_id to make sure they are the sample - increment error counter
        if(build_all_samples)
          ps = S_WRITE0;
        else if((sum < sum_thr) || max_pos0_test.and_reduce() || max_pos5_test.and_reduce())
        {
          if(sample_n++ == 127)
            ps = S_HEADER;
          else
            ps = S_READ0;
        }
        else
          ps = S_WRITE0;
      }
      break;

    case S_WRITE0:
      // Write samples 0,1
      event_data.data(12,0)  = s[0];
      event_data.data(25,13) = s[1];
      event_data.data(30,26) = sample_n(4,0);
      event_data.data(31,31) = 0;
      event_data.end = 0;
      s_evOut.write(event_data);

      ps = S_WRITE1;
      break;

    case S_WRITE1:
      // Write samples 2,3
      event_data.data(12,0)  = s[2];
      event_data.data(25,13) = s[3];
      event_data.data(30,26) = sample_n(6,5);
      event_data.data(31,31) = 0;
      event_data.end = 0;
      s_evOut.write(event_data);

      ps = S_WRITE2;
      break;

    case S_WRITE2:
      // Write samples 4,5
      event_data.data(12,0)  = s[4];
      event_data.data(25,13) = s[5];
      event_data.data(30,26) = apv_id;
      event_data.data(31,31) = 0;
      event_data.end = 0;
      s_evOut.write(event_data);

      if(sample_n++ == 127)
        ps = S_HEADER;
      else
        ps = S_READ0;
      break;
  }
}

void avgBSamplesFifoProc(
    hls::stream<sample_data_t> &s_avgBSamplesOut,
    hls::stream<sample_data_pair_t> &s_avgBSamplesIn
  )
{
  static hls::stream<sample_data_t> s_avgBSamples[6];
  static int wr_pos = 0, wr_idx = 0;

  if(!s_avgBSamplesOut.empty())
  {
    sample_data_t val = s_avgBSamplesOut.read();
    s_avgBSamples[wr_idx].write(val);

    if(wr_idx==5)
    {
      sample_data_pair_t pair_val;
      for(int i=0;i<6;i+=2)
      {
        pair_val.data[0] = (s_avgBSamples[i+0].read()).data;
        pair_val.data[1] = (s_avgBSamples[i+1].read()).data;
        s_avgBSamplesIn.write(pair_val);
      }
    }

    if(wr_pos==127)
      wr_idx=(wr_idx+1)%6;

    wr_pos=(wr_pos+1)%128;
  }

}

//from Xinzhan's

inline void find_max(
	ap_int<13> min_vals,
    ap_uint<5> max_pos,
    ap_int<13> max
  )
{
  ap_int<13> max_vars[4];
  ap_uint<5> max_pos_vars[4];

  for(int j=0;j<4;j++)
  {
    for(int i=0;i<5;i++)
    {
      if(!i || (min_vals[i+j*5] > max_vars[j]))
      {
        max_pos_vars[j] = i+j*5;
        max_vars[j] = min_vals[i+j*5];
      }
    }
  }

  if( (max_vars[0] >= max_vars[1]) && (max_vars[0] >= max_vars[2]) && (max_vars[0] >= max_vars[3]) )
  {
    max_pos = max_pos_vars[0];
    max = max_vars[0];
  }
  else if( (max_vars[1] >= max_vars[0]) && (max_vars[1] >= max_vars[2]) && (max_vars[1] >= max_vars[3]) )
  {
    max_pos = max_pos_vars[1];
    max = max_vars[1];
  }
  else if( (max_vars[2] >= max_vars[0]) && (max_vars[2] >= max_vars[1]) && (max_vars[2] >= max_vars[3]) )
  {
    max_pos = max_pos_vars[2];
    max = max_vars[2];
  }
  else
  {
    max_pos = max_pos_vars[3];
    max = max_vars[3];
  }
}

void apv_sorting_hls(
    hls::stream<sample_data_t> &s_apv_samples,
    hls::stream<apv_common_mode_t> &s_apv_common_mode
  )
{
  enum {S_INIT, S_PRELOAD, S_FIND_MAX, S_TEST_AND_STORE};
  static ap_int<13> min_vals[20];
  static int cnt, ps = S_INIT;
  static ap_uint<5> max_pos;
  static ap_int<13> max;
  static ap_int<18> sum;


  switch(ps)
  {
  	  case S_INIT:
    	if(!s_apv_samples.empty()){
    	      sum = 0;
    	      cnt = 0;
    	      ps = S_PRELOAD;
    }
      break;

    case S_PRELOAD:
    {
      sample_data_t s= s_apv_samples.read();
      min_vals[cnt++] = s.data;
      sum+= s.data;
      if(cnt==20)
        ps = S_FIND_MAX;
      break;
    }

    case S_FIND_MAX:
    {
      find_max(min_vals, &max_pos, &max);
      ps = S_TEST_AND_STORE;
      break;
    }

    case S_TEST_AND_STORE:
    {
      // Test/store new sample
      sample_data_t s = s_apv_samples.read();
      if(s.data < max)
      {
        sum = sum-max+s.data;
        min_vals[max_pos] = s.data;
      }

      if(cnt==127)
      {
        s_apv_common_mode.write(sum);
        ps = S_INIT;
      }
      else
      {
        cnt++;
        ps = S_FIND_MAX;
      }

      break;
    }
  }
}


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
  )
{
  static hls::stream<avg_pre_header_t> s_avgAPreHeader, s_avgBPreHeader;
  static hls::stream<avg_header_t> s_avgAHeader, s_avgBHeader;
  static hls::stream<sample_data_t> s_avgASamples, s_avgBSamplesOut,s_apv_Samples //not using this for the time being;
  static hls::stream<sample_data_pair_t> s_avgBSamplesIn;
  static hls::stream<apv_common_mode_t> s_apv_common_mode;

  frame_decoder(s_evIn, s_avgASamples, m_offset, s_avgAPreHeader);

  avgHeaderDiv(s_avgAPreHeader, s_avgBPreHeader, s_avgAHeader, s_avgBHeader);

  avgB(s_avgASamples, s_avgAHeader, s_avgBSamplesOut, s_avgBPreHeader, m_apvThrB);

  avgBSamplesFifoProc(s_avgBSamplesOut, s_avgBSamplesIn);

  event_writer(s_evOut, s_avgBHeader, s_avgBSamplesIn, build_all_samples, enable_cm, fiber, m_apvThr);
  
  apv_sorting_hls(s_avgASamples,s_apv_common_mode); //here I am using s_avgASamples because I assumed that its the output from frame_decoder going into the avgB
}
