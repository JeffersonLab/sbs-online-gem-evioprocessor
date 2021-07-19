#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mpd_data_processor.h"
#include "mpd_data_processor_wrapper.h"

#define RAND_SEED(i)  srand(i)
#define RAND_NUM()    ((double)rand() / RAND_MAX)

apvEvent_t gApvEvent;
//vimukthi
apvEvent_t apvEvent_new;
apvEvent_t apvEvent_sorted;
corr bcorr; 

double rand_norm(double mean, double std)
{
  double u = RAND_NUM();
  double v = RAND_NUM();
  return mean+std*sqrt(-2.0*log(u))*cos(2*M_PI*v);
}

int main(int argc, char *argv[])
{
  int t_fiber = 0;
  int t_apvmask = 0x0003;
  double t_cm_min = 300;
  double t_cm_max = 800;
  double t_offset_range = 30;
  double t_noise = 10;
  double t_thr = 50;
  double t_hit_prob = 0.10; // chance to generate hit
  double t_hit_amp = 2000;
  double pulse[18] = {
      0.00, 0.00, 0.00, 0.00, 0.00, 0.00,
      0.00, 0.60, 0.90, 0.70, 0.40, 0.10,
      0.00, 0.00, 0.00, 0.00, 0.00, 0.00
    };

  RAND_SEED(1);

  // Clear thresholds/offsets
  memset(offset_All, 0, sizeof(offset_All));
  memset(avgAmin_All, 0, sizeof(avgAmin_All));
  memset(avgAmax_All, 0, sizeof(avgAmax_All));
  memset(apvThr_All, 0, sizeof(apvThr_All));

  // setup test fiber thresholds/offsets
  for(int i=0;i<APV_NUM_MAX;i++)
  {
    if(t_apvmask & (1<<i))
    {
      avgAmin_All[t_fiber][i] = t_cm_min;
      avgAmax_All[t_fiber][i] = t_cm_max;
      for(int j=0;j<APV_STRIPS;j++)
      {
        offset_All[t_fiber][i][j] = RAND_NUM()*t_offset_range;
        apvThr_All[t_fiber][i][j] = t_thr;
      }
    }
  }

  // Clear event structure (disables all)
  memset(&gApvEvent, 0, sizeof(gApvEvent));

  // Fill in APV/SSP event info for 1 fiber to test
  gApvEvent.slot = 3;                     // SSP slot
  gApvEvent.event_time = 0;               // SSP event timestamp
  gApvEvent.event_number = 1;             // SSP event number
  gApvEvent.valid[t_fiber] = t_apvmask;   // APVID valid IDs (0x7FFF => first 15 APV on fiber valid)
  gApvEvent.mpdid[t_fiber] = t_fiber;     // MPDID, just set equal to FiberID

  // generate random hits/noise
  for(int i=0;i<APV_NUM_MAX;i++)
  {
    if(t_apvmask & (1<<i))
    {
      double cm = t_cm_min+RAND_NUM()*(t_cm_max-t_cm_min);
      for(int j=0;j<APV_STRIPS;j++)
      {
        if(RAND_NUM() < t_hit_prob)
        {
          double a = RAND_NUM()*t_hit_amp;
          int t = RAND_NUM()*12.0; // random time/phase offset
          for(int k=0;k<APV_SAMPLE_MAX;k++)
            gApvEvent.data[t_fiber][i][j][k] = cm+rand_norm(0,t_noise)+a*pulse[k+t];
        }
        else
        {
          for(int k=0;k<APV_SAMPLE_MAX;k++)
            gApvEvent.data[t_fiber][i][j][k] = cm+rand_norm(0,t_noise);
        }
      }
    }
  }

  float threshold_rms_factor = 3.0;
  int build_all_samples = 0;
  int build_debug_headers = 0;
  int enable_cm = 1;

  cout << "### Original Event ###" << endl;
  mpdssp_PrintEvent(&gApvEvent);

  vector<uint32_t> sspEvt_vec = mpdssp_ProcessEvent(
      &gApvEvent,
      threshold_rms_factor,
      build_all_samples,
      build_debug_headers,
      enable_cm
    );

  apvEvent_t apvEvent_new;
  mpdssp_DecodeEvent(&sspEvt_vec, &apvEvent_new);

  cout << "### Processed Event ###" << endl;
  mpdssp_PrintEvent(&apvEvent_new);

  // process gApvEvent here with simple algorithm and compare directly against apvEvent_new for any errors
  // can loop this process (random event, hw algorithm process, sw algorithm process, check)

  //Vimukthi
  
  memcpy(&apvEvent_sorted, &gApvEvent, sizeof(apvEvent_t));

  BstNode*rootf=NULL;

  commonmode_substraction(&apvEvent_sorted,rootf,&bcorr);

  cout << "### Processed Event Cmsorted Method###" << endl;
  mpdssp_PrintEvent(&apvEvent_sorted);

  return 0;
