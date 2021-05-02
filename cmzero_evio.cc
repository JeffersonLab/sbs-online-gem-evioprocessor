#include <evioUtil.hxx>
#include <evioFileChannel.hxx>
#include <string>
#include <vector>
#include <sys/unistd.h>
#include "mpd_data_processor_wrapper.h"

using namespace std;
using namespace evio;

#define MPD_SSP_BANK      10

#define PRINT_EVENT_ORIG      0
#define PRINT_EVENT_SSP       0
#define ENABLE_SSP_PROCESSING 1
#define PAUSE_AFTER_EVENT     0
#define WRITE_EVIO_OUTPUT     1

apvEvent_t ApvEvent_orig;
apvEvent_t ApvEvent_ssp;


void clearEventBuffers()
{
  // Clear APV event buffer
  memset(ApvEvent_orig.valid, 0, sizeof(ApvEvent_orig.valid));
  memset(ApvEvent_ssp.valid, 0, sizeof(ApvEvent_ssp.valid));
}

void eventProc(const evioDOMNodeP pNode)
{
	static int roc = -1;
	int type = pNode->getContentType();
	
	if(type == 1) // raw 32bit evio bank
	{
		switch(pNode->tag)
		{
      case MPD_SSP_BANK:
        mpdssp_DecodeEvent(pNode->getVector<uint32_t>(), &ApvEvent_orig);
#if PRINT_EVENT
        mpdssp_PrintEvent(&ApvEvent_orig);
#endif

#if ENABLE_SSP_PROCESSING
        // Processing settings
        float threshold_rms_factor = 5.0;
        int   build_all_samples = 1;
        int   build_debug_headers = 0;
        int   enable_cm = 1;

        vector<uint32_t> uivec = mpdssp_ProcessEvent(&ApvEvent_orig, threshold_rms_factor, build_all_samples, build_debug_headers, enable_cm);

        mpdssp_DecodeEvent(&uivec, &ApvEvent_ssp);
#if PRINT_EVENT_SSP
        mpdssp_PrintEvent(&ApvEvent_ssp);
#endif
        evioDOMNodeP newNode = evioDOMNode::createEvioDOMNode(100,0,uivec);
        pNode->getParent()->addNode(newNode);
#endif
        break;
		}
	}
	else if(type == 14) // roc id
    roc = pNode->tag;
	else if(type == 15) // composite evio bank
  {
	}
}

int main(int argc, char **argv)
{
  if(argc<4)
  {
    printf("usage: cmzero_evio <evio_input_file> <pedestal_file> <common-mode_file>\n");
    return -1;
  }

  LoadPedestals(argv[2]);
  LoadCommonMode(argv[3]);

	int nread = 0;
	try
	{
    string evioIn = argv[1];
#if WRITE_EVIO_OUTPUT
    string evioOut = evioIn + "_out";
#endif

    cout << "opening evio file: " << evioIn;
    if( (access(evioIn.c_str(), R_OK) == -1) )
    {
      cout << "...failed." << endl;
      goto complete;
    }

    evioFileChannel *chan_in = new evioFileChannel((const char *)evioIn.c_str(),"r");
    chan_in->open();
#if WRITE_EVIO_OUTPUT
    cout << ", " << evioOut;
    evioFileChannel *chan_out = new evioFileChannel((const char *)evioOut.c_str(), "w");
    chan_out->open();
#endif
    cout << endl;

    while(chan_in->read())
    {
      if(!(nread % 1))
        cout << " --- processing event " << nread << " ---" << endl;

      clearEventBuffers();

      // Read in event
      evioDOMTree event(chan_in);
      evioDOMNodeListP fullList = event.getNodeList();
      for_each(fullList->begin(), fullList->end(), eventProc);

#if WRITE_EVIO_OUTPUT
      // Write back out (contains new bank data)
      chan_out->write(event);
#endif

      nread++;
#if PAUSE_AFTER_EVENT
      getchar();
#endif
    }
    chan_in->close();
    delete(chan_in);
#if WRITE_EVIO_OUTPUT
    chan_out->close();
    delete(chan_out);
#endif
	}
	catch(evioException e)
	{
		cerr << e.toString() << endl;
	}
	catch(...)
	{
		cerr << "?unknown exception" << endl;
	}
	
complete:
  return 0;
}

