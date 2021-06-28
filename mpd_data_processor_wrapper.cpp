#include <fstream>
#include <string>
#include <ap_int.h>
#include <hls_stream.h>
#include "mpd_data_processor_wrapper.h"

// Settings from files
int offset_All[FIBER_NUM][APV_NUM_MAX][APV_STRIPS];
int avgAmin_All[FIBER_NUM][APV_NUM_MAX];
int avgAmax_All[FIBER_NUM][APV_NUM_MAX];
float apvThr_All[FIBER_NUM][APV_NUM_MAX][APV_STRIPS];


void mpdssp_DecodeEvent(vector<uint32_t> *vec, apvEvent_t *evt)
{
  enum {S_HEADER, S_APV_W0, S_APV_W1, S_APV_W2};
	int tag=-1, idx, fiber, mpdid, state, apv_w[3], apvid, apvch, s[6];

  memset(evt, 0, sizeof(apvEvent_t));

  if(vec->size() <= 0)
    return;

	for(int i=0;i<vec->size();i++)
	{
    unsigned int val = (*vec)[i];

    if( (val>>31) & 0x1 )
    {
      tag = (val>>27) & 0xF;
      idx = 0;
    }
    else
      idx++;

    if(tag<0)
      continue;  

    switch(tag)
    {
      case 0: // Block header
      {
        break;
      } // case 0

      case 2: // Event header
      {
        if(idx==0)
        {
          evt->event_number = (val>>0) & 0x3FFFFF;
          evt->slot = (val>>22) & 0x1F;
        }
        break;
      } // case 2

      case 3: // Trigger number
      {
        if(idx==0)
          evt->event_time = (val>>0) & 0xFFFFFF;
        else if(idx==1)
          evt->event_time|= ((uint64_t)((val>>0) & 0xFFFFFF))<<24LL;
        break;
      } // case 3

      case 5: // APV Data
      {
        if(idx==0)
          state = S_HEADER;

        switch(state)
        {
          case S_HEADER: // Fiber, MPD_ID
            fiber = (val>>16) & 0x1F;
            mpdid = (val>> 0) & 0x1F;
            state = S_APV_W0;
            break;

          case S_APV_W0:
            apv_w[0] = val;
            state = S_APV_W1;
            break;

          case S_APV_W1:
            apv_w[1] = val;
            state = S_APV_W2;
            break;

          case S_APV_W2:
            apv_w[2] = val;
            state = S_APV_W0;

            apvid = (apv_w[2]>>26) & 0x1F;
            apvch = ((apv_w[0]>>26) & 0x1F) | (((apv_w[1]>>26) & 0x3)<<5);

            s[0]  = (apv_w[0]>>0) & 0x1FFF;
            s[1]  = (apv_w[0]>>13) & 0x1FFF;
            s[2]  = (apv_w[1]>>0) & 0x1FFF;
            s[3]  = (apv_w[1]>>13) & 0x1FFF;
            s[4]  = (apv_w[2]>>0) & 0x1FFF;
            s[5]  = (apv_w[2]>>13) & 0x1FFF;
        
            evt->mpdid[fiber] = mpdid;    
            evt->valid[fiber]|= 1<<apvid;
            for(int i=0;i<6;i++)
            {
              if(s[i] & 0x1000) s[i] |= 0xFFFFE000; // sign extend
              evt->data[fiber][apvid][apvch][i] = s[i];
            }
            break;
        }
        break;
      } // case 5
    }
	}
}


void mpdssp_PrintEvent(apvEvent_t *evt)
{
  printf("%s: [START OF EVENT]\n", __func__);
  for(int i=0;i<FIBER_NUM;i++)
  for(int j=0;j<APV_NUM_MAX;j++)
  {
    if(!(evt->valid[i] & (1<<j)))
      continue;

    printf("[SSP FIBER %d,%d]\n", i, j);
    for(int k=0;k<APV_STRIPS;k++)
    {
      if( evt->data[i][j][k][0] || evt->data[i][j][k][1] ||
          evt->data[i][j][k][2] || evt->data[i][j][k][3] ||
          evt->data[i][j][k][4] || evt->data[i][j][k][5] )
      {
        printf("APV%2d, CH%3d: %4d %4d %4d %4d %4d %4d\n", j,k,
            evt->data[i][j][k][0], evt->data[i][j][k][1],
            evt->data[i][j][k][2], evt->data[i][j][k][3],
            evt->data[i][j][k][4], evt->data[i][j][k][5]
          );
      }
    }
  }
  printf("%s: [END OF EVENT]\n", __func__);
}


void LoadCommonMode(const char *filename)
{
  if( (access(filename, R_OK) == -1) )
  {
    cout << "Failed to open common-mode file: " << filename << endl;
    return;
  }

  int mpd_id, apv_id, t_cm_min, t_cm_max;
  std::ifstream f(filename);
  while(f>>mpd_id>>apv_id>>t_cm_min>>t_cm_max){
    avgAmin_All[mpd_id][apv_id] = t_cm_min;
    avgAmax_All[mpd_id][apv_id] = t_cm_max;
  }
  f.close();
}


void LoadPedestals(const char *filename)
{
  if( (access(filename, R_OK) == -1) )
  {
    cout << "Failed to open pedestal file: " << filename << endl;
    return;
  }

  int mpd_id, apv_id, strip_id, t_offset, slot;
  float t_rms;
  char buf[100];

  std::ifstream f(filename);
  while(f.getline(buf, sizeof(buf)))
  {
    vector<string> tokens;

    int slot = -1, fiber = -1, apv = -1;
    char *pbuf = strtok(buf," ");
    while(pbuf)
    {
      tokens.push_back(pbuf);
      pbuf = strtok(NULL, " ");
    }

    if( (tokens.size()==4) && !tokens[0].compare("APV") )
    {
      slot = atoi(tokens[1].c_str());
      mpd_id = atoi(tokens[2].c_str());
      apv_id = atoi(tokens[3].c_str());
    }
    else if(tokens.size()==3)
    {
      strip_id = atoi(tokens[0].c_str());
      t_offset = atoi(tokens[1].c_str());
      t_rms = atof(tokens[2].c_str());

      if( (mpd_id>=0) && (mpd_id<32) && (apv_id>=0) && (apv_id<15) && (strip_id>=0) && (strip_id<128) )
      {
        offset_All[mpd_id][apv_id][strip_id] = t_offset;
        apvThr_All[mpd_id][apv_id][strip_id] = t_rms;
      }
      else
      {
        cout << "Pedestal file error: " <<
                "mpd_id=" << mpd_id <<
                ",apv_id=" << apv_id <<
                ",strip_id=" << strip_id <<
                ",offset=" << t_offset <<
                ",rms=" << t_rms << endl;
        f.close();
        return;
      }
    }
  }
  f.close();
}



vector<uint32_t> mpdssp_ProcessEvent(
    apvEvent_t *evt,
    float threshold_rms_factor,
    int build_all_samples,
    int build_debug_headers,
    int enable_cm
  )
{
  vector<uint32_t> uivec;
  int any_valid = 0, val;

  for(int i=0;i<FIBER_NUM;i++)
  {
    if(evt->valid[i])
      any_valid = 1;
  }

  if(any_valid)
  {
    // SSP block header
    val = 0x80000000 | (evt->slot<<22) | ((evt->event_number&0x3FF)<<8) | 0x1;
    uivec.push_back(val);

    // SSP event header
    val = 0x90000000 | (evt->slot<<22) | ((evt->event_number&0x3FFFF)<<0);
    uivec.push_back(val);

    // SSP event time 0
    val = 0x98000000 | ((evt->event_time&0xFFFFFF)<<0);
    uivec.push_back(val);

    // SSP event time 1
    val = (evt->event_time>>24LL) & 0xFFFFFF;
    uivec.push_back(val);
  }
  else
    return uivec;   

  for(int i=0;i<FIBER_NUM;i++)
  {
    if(!evt->valid[i])
      continue;

    // Load settings for mpd data processing
    ap_uint<5> fiber = i;
    ap_int<26> m_offset[APV_NUM_MAX][APV_STRIPS/2];
    ap_int<13> m_apvThr[APV_NUM_MAX][APV_STRIPS];

    for(int a=0;a<APV_NUM_MAX;a++)
    for(int ch=0;ch<APV_STRIPS;ch++)
    {
      float thr = apvThr_All[i][a][ch] * threshold_rms_factor;
      m_apvThr[a][ch] = (ap_int<13>)thr;
    }

    for(int a=0;a<APV_NUM_MAX;a++)
    for(int ch=0;ch<APV_STRIPS;ch++)
    {
      if(a<15)
      {
        if(!(ch%2))
        {
          m_offset[a][ch/2](12, 0) = offset_All[i][a][ch+0];
          m_offset[a][ch/2](25,13) = offset_All[i][a][ch+1];
        }
      }
      else
      {
        // Common-mode min/max settings (stored at non-existing APVID=15)
        if(ch<16)
        {
          m_offset[a][ch](12, 0) = avgAmin_All[i][ch];
          m_offset[a][ch](25,13) = avgAmax_All[i][ch];
        }
        else if(ch < 64)
          m_offset[a][ch] = 0;
      }
    }

    hls::stream<event_data_t> evIn;
    hls::stream<event_data_t> evOut;
    event_data_t eventData;

    // Load MPD formatted event data stream
    eventData.data(31,27) = 0x10;   // Block header
    eventData.data(26,21) = 0;      // reserved
    eventData.data(20,16) = evt->mpdid[i];  // mpdid
    eventData.data(15, 0) = 0;      // reserved
    eventData.end = 0;
    evIn.write(eventData);

    for(int a=0;a<APV_NUM_MAX;a++)
    {
      if(!(evt->valid[i] & (1<<a)))
        continue;

      for(int t=0;t<APV_SAMPLE_MAX;t++)
      {
        eventData.data(31,27) = 0x14;   // APV header
        eventData.data(26,23) = a;      // apvid
        eventData.data(22, 0) = 0;      // reserved
        eventData.end = 0;
        evIn.write(eventData);

        for(int ch=0;ch<128;ch+=2)
        {
          eventData.data(12, 0) = evt->data[fiber][a][ch+0][t];
          eventData.data(25,13) = evt->data[fiber][a][ch+1][t];
          eventData.data(31,26) = 0;    // reserved
          eventData.end = 0;
          evIn.write(eventData);
        }
      }
    }
    
    eventData.end = 1;              // End of event
    eventData.data(31, 0) = 0;      // reserved
    evIn.write(eventData);


    // Processing loop
    while(true)
    {
      mpd_data_processor_main(
          evIn, evOut,
          build_all_samples, build_debug_headers, enable_cm, fiber,
          m_offset, m_apvThr, m_apvThr
        );

      if(!evOut.empty())
      {
        event_data_t eventData = evOut.read();

        if(eventData.end)
        {
          if(!evOut.empty()) cout << "ERROR - evOut not empty on end of event!!!\n";
          if(!evIn.empty()) cout << "ERROR - evIn not empty on end of event!!!\n";
          break;
        }
        uivec.push_back(eventData.data.to_uint());
      }
    }
  }
  return uivec;
}

