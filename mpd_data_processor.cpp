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
    hls::stream<sample_data_t> (&s_sample_data_array)[6],
    ap_int<26> m_offset[APV_NUM_MAX][APV_STRIPS/2],
    hls::stream<header_t> &s_header
  )
{
  static enum eState {S_BLOCK_HEADER, S_APV_HEADER, S_APV_DATA, S_APV_TRAILER, S_ERROR} ps = S_BLOCK_HEADER;
  static ap_uint<6> adc_word_cnt;
  static ap_uint<3> time_cnt;
  static header_t header;
  event_data_t val;

  switch(ps)
  {
    case S_BLOCK_HEADER:
      if(!s_evIn.empty())
      {
        val = s_evIn.read();
        if(val.data(31,27) == BLOCK_HEADER)
        {
          time_cnt = 0;
          header.mpdid = val.data(20,16);
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
          header.mpdid = 0;
          header.apvid = 0;
          header.evtend = 1;
          s_header.write(header);
          ps = S_BLOCK_HEADER;
        }
        else if(val.data(31,27) == APV_CH_DATA)
        {
          header.apvid = val.data(26,23);
          header.evtend = 0;
          adc_word_cnt = 0;

          ps = S_APV_DATA;
        }
      }
      break;

    case S_APV_DATA:
      if(!s_evIn.empty())
      {
        sample_data_t sample_data;
        ap_fixed<13,13,AP_RND,AP_SAT> adc;
        ap_uint<26> offset_data = m_offset[header.apvid][adc_word_cnt];

        val = s_evIn.read();

        //Sample0
        adc = (ap_int<13>)val.data(12,0) - (ap_int<13>)offset_data(12,0);
        sample_data.data = (ap_int<13>)adc;
        s_sample_data.write(sample_data);
        s_sample_data_array[time_cnt].write(sample_data);

        //Sample1
        adc = (ap_int<13>)val.data(25,13) - (ap_int<13>)offset_data(25,13);
        sample_data.data = (ap_int<13>)adc;
        s_sample_data.write(sample_data);
        s_sample_data_array[time_cnt].write(sample_data);

        if(adc_word_cnt == 63)
          ps = S_APV_TRAILER;

        adc_word_cnt++;
      }
      break;

    case S_APV_TRAILER:
      time_cnt++;
      if(time_cnt>5)
        time_cnt = 0;
      s_header.write(header);
      ps = S_APV_HEADER;
      break;

    case S_ERROR:
      break;
  }
}

void avgHeaderDiv(
    hls::stream<apv_common_mode_sum_t> &s_apv_common_mode_sum,
    hls::stream<apv_common_mode_avg_t> &s_apv_common_mode_avg
  )
{
  if(!s_apv_common_mode_sum.empty())
  {
    apv_common_mode_sum_t sum = s_apv_common_mode_sum.read();
    apv_common_mode_avg_t avg = sum / 20;
    s_apv_common_mode_avg.write(avg);
  }
}

void event_writer(
    hls::stream<event_data_t> &s_evOut,
    hls::stream<header_t> &s_header,
    hls::stream<apv_common_mode_avg_t> &s_apv_common_mode_avg,
    hls::stream<sample_data_t> (&s_SamplesPar)[6],
    ap_uint<1> build_all_samples,
    ap_uint<1> enable_cm,
    ap_uint<5> fiber,
    ap_int<13> m_apvThr[APV_NUM_MAX][APV_STRIPS]
  )
{
  static enum eState {S_HEADER, S_HEADER_N, S_CM_AVG, S_READ0, S_READ1, S_READ2, S_WRITE0, S_WRITE1, S_WRITE2, S_ERROR} ps = S_HEADER;
  static ap_uint<4> apvid;
  static ap_int<13> avg[6];
#pragma HLS ARRAY_PARTITION variable=avg complete dim=1
  static ap_fixed<13,13,AP_RND,AP_SAT> s[6];
#pragma HLS ARRAY_PARTITION variable=s complete dim=1
  static ap_uint<7> sample_n;
  static ap_uint<3> cnt = 0;
  static ap_int<16> sum, sum0, sum1, sum2;
  static header_t header;
  event_data_t event_data;

  switch(ps)
  {
    case S_HEADER:
    {
      if(!s_header.empty())
      {
        header = s_header.read();
        if(header.evtend)
        {
          event_data.data = 0xFFFFFFFF;
          event_data.end = 1;
          s_evOut.write(event_data);
        }
        else
        {
          if(!cnt)
          {
            apvid = header.apvid;
            event_data.data(31,27) = 0x15;
            event_data.data(26,21) = 0;
            event_data.data(20,16) = fiber;
            event_data.data(15,5)  = 0;
            event_data.data(4,0)   = header.mpdid;
            event_data.end = 0;
            s_evOut.write(event_data);
            ps = S_CM_AVG;
          }
        }
      }
      break;
    }

    case S_HEADER_N:
    {
      if(!s_header.empty())
      {
        header = s_header.read();
        if(header.evtend)
          ps = S_ERROR;
        else
          ps = S_CM_AVG;
      }
      break;
    }

    case S_CM_AVG:
    {
      if(!s_apv_common_mode_avg.empty())
      {
        avg[cnt++] = s_apv_common_mode_avg.read();
        if(cnt == 6)
        {
          sample_n = 0;
          cnt = 0;
          ps = S_READ0;
        }
        else
          ps = S_HEADER_N;
      }
      break;
    }

    case S_READ0:
    {
      if(!s_SamplesPar[0].empty() && !s_SamplesPar[1].empty() && !s_SamplesPar[2].empty() && !s_SamplesPar[3].empty() && !s_SamplesPar[4].empty() && !s_SamplesPar[5].empty())
      {
        for(int i=0;i<6;i++)
        {
#pragma HLS UNROLL
          sample_data_t data = s_SamplesPar[i].read();
          s[i] = enable_cm ? (ap_fixed<13,13,AP_RND,AP_SAT>)(data.data - avg[i]) : (ap_fixed<13,13,AP_RND,AP_SAT>)data.data;
        }

        sum0 = s[0] + s[1];
        sum1 = s[4] + s[5];
        sum2 = s[2] + s[3];
        ps = S_READ1;
      }
      break;
    }

    case S_READ1:
    {
      sum = sum0+sum1+sum2;
      ps = S_READ2;
      break;
    }

    case S_READ2:
    {
      ap_uint<5> max_pos0_test, max_pos5_test;
      ap_uint<16> sum_thr = 6 * m_apvThr[apvid][sample_n];

      for(int i=0;i<5;i++)
      {
#pragma HLS UNROLL
        max_pos0_test[i] = (s[0] > s[1+i]) ? 1 : 0;
        max_pos5_test[i] = (s[5] > s[i]  ) ? 1 : 0;
      }

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

      break;
    }

    case S_WRITE0:
    {
      // Write samples 0,1
      event_data.data(12,0)  = s[0];
      event_data.data(25,13) = s[1];
      event_data.data(30,26) = sample_n(4,0);
      event_data.data(31,31) = 0;
      event_data.end = 0;
      s_evOut.write(event_data);

      ps = S_WRITE1;
      break;
    }

    case S_WRITE1:
    {
      // Write samples 2,3
      event_data.data(12,0)  = s[2];
      event_data.data(25,13) = s[3];
      event_data.data(30,26) = sample_n(6,5);
      event_data.data(31,31) = 0;
      event_data.end = 0;
      s_evOut.write(event_data);

      ps = S_WRITE2;
      break;
    }

    case S_WRITE2:
    {
      // Write samples 4,5
      event_data.data(12,0)  = s[4];
      event_data.data(25,13) = s[5];
      event_data.data(30,26) = apvid;
      event_data.data(31,31) = 0;
      event_data.end = 0;
      s_evOut.write(event_data);

      if(sample_n++ == 127)
        ps = S_HEADER;
      else
        ps = S_READ0;
      break;
    }

    case S_ERROR:
    {
      break;
    }
  }
}

//from Xinzhan's

inline void find_max(
	ap_int<13> min_vals[20],
    ap_uint<5> max_pos,
    ap_int<13> max
  )
{
  ap_int<13> max_vars[4];
  ap_uint<5> max_pos_vars[4];
  std::cout<<"M1"<<"\n";
  for(int j=0;j<4;j++)
  { std::cout<<"j: "<<j<<"\n";
    for(int i=0;i<5;i++)
    { //std::cout<<"M3";
      if(!i || (min_vals[i+j*5] > max_vars[j]))
      {
        max_pos_vars[j] = i+j*5;
        max_vars[j] = min_vals[i+j*5];
        std::cout<<max_vars[j]<<"\n";
      }
    }
  }
  //std::cout<<"M5";
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
    hls::stream<sample_data_t> &s_sample_data,
    hls::stream<apv_common_mode_sum_t> &s_apv_common_mode
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
  	{
  		std::cout <<"A";
    	if(!s_sample_data.empty()){
    	      sum = 0;
    	      cnt = 0;
    	      std::cout <<"B";
    	      ps = S_PRELOAD;
    	}
  	}
    	break;

    case S_PRELOAD:
    {
      std::cout<<"C";
      if(!s_sample_data.empty()){
    	  std::cout<<cnt;
		  sample_data_t s= s_sample_data.read();
		  min_vals[cnt++] = s.data;
		  sum+= s.data;
		  if(cnt==20)
			//std::cout<<cnt;
			ps = S_FIND_MAX;
      }
      break;
    }

    case S_FIND_MAX:
    {

	  find_max(min_vals, &max_pos, &max);
	  std::cout<<"E";
	  ps = S_TEST_AND_STORE;
	  break;
    }

    case S_TEST_AND_STORE:
    {
      // Test/store new sample
      if(!s_sample_data.empty()){
    	  std::cout<<"F";
		  sample_data_t s = s_sample_data.read();

		  if(s.data < max)
		  {
			sum = sum-max+s.data;
			min_vals[max_pos] = s.data;
		  }

		  if(cnt==127)
		  {	std::cout<<"sum: "<<sum<<"\n";
			s_apv_common_mode.write(sum);
			ps = S_INIT;
		  }
		  else
		  {
			cnt++;
			std::cout<<"cnt: "<<cnt;
			ps = S_FIND_MAX;
		  }
      }
      break;
    }
  }
}

/*
 * e_in:  MPD event data input
 * s:     APV samples
 * s[6]:  APV samples for all 6 time samples
 * h:     Header APV/MPD ID
 * sum:   Divider input (sum)
 * avg:   Divider output (avg=sum/20)
 * e_out: Processed event data output

   |e_in
   |
 frame_decoder(
   |     |     |
   |s[6] |h    |s
   |     |     |
   |     |   apv_sorting_hls
   |     |     |
   |     |     |sum
   |     |     |
   |     |   avgHeaderDiv
   |     |     |
   |     |     |avg
   |     |     |
 event_writer
   |
   |e_out

*/

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
  static hls::stream<sample_data_t> s_sample_data, s_sample_data_array[6];
#pragma HLS STREAM variable=s_sample_data depth=128 dim=1
#pragma HLS STREAM variable=s_sample_data_array depth=128
  static hls::stream<apv_common_mode_sum_t> s_apv_common_mode_sum;
  static hls::stream<apv_common_mode_avg_t> s_apv_common_mode_avg;
  static hls::stream<header_t> s_header;

  event_writer(s_evOut, s_header, s_apv_common_mode_avg, s_sample_data_array, build_all_samples, enable_cm, fiber, m_apvThr);

  apv_sorting_hls(s_sample_data,s_apv_common_mode_sum);

  avgHeaderDiv(s_apv_common_mode_sum, s_apv_common_mode_avg);

  frame_decoder(s_evIn, s_sample_data, s_sample_data_array, m_offset, s_header);
}
