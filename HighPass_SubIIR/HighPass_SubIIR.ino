#include <MP.h>

#include "IIR.h"

/* Use CMSIS library */
#define ARM_MATH_CM4
#define __FPU_PRESENT 1U
#include <cmsis/arm_math.h>

/* Parameters */
const int   g_channel = 1; /* Number of channels */
const int   g_cutoff  = 1000; /* Cutoff frequency */
const float g_Q       = sqrt(0.5); /* Q Value */
const int   g_sample  = 240; /* Number of channels */
const int   g_fs      = 48000; /* Sampling Rate */

const int   g_result_size = 4; /* Result buffer size */

IIRClass HPF;

/* Allocate the larger heap size than default */
USER_HEAP_SIZE(64 * 1024);

/* MultiCore definitions */
struct Request {
  void *buffer;
  int  sample;
};

struct Result {
  void *buffer;
  int  sample;
};

void setup()
{
  int ret = 0;

  /* Initialize MP library */
  ret = MP.begin();
  if (ret < 0) {
    errorLoop(2);
  }
  /* receive with non-blocking */
  MP.RecvTimeout(100000);

  /* begin Filter */
  if(!HPF.begin(TYPE_HPF, g_channel, g_cutoff, g_Q, g_sample, IIRClass::Interleave, g_fs)) {
    int err = HPF.getErrorCause();
    printf("error! %d\n", err);
    errorLoop(abs(err));
  }
}

void loop()
{
  int      ret;
  int8_t   sndid = 10; /* user-defined msgid */
  int8_t   rcvid;
  Request  *request;
  static Result result[g_result_size];

  static q15_t pDst[g_sample * sizeof(q15_t) * g_channel];
  static q15_t out_buffer[g_result_size][g_sample * sizeof(q15_t) * g_channel];
  static int pos = 0;

  /* Receive PCM captured buffer from MainCore */
  ret = MP.Recv(&rcvid, &request);
  if (ret >= 0) {
    HPF.put((q15_t*)request->buffer, request->sample);
  }else{
    printf("error! %d\n", ret);
    return;
  }

  int cnt = 0;
  while (!HPF.empty(0)) {
    cnt = HPF.get(pDst);
    memcpy(&out_buffer[pos][0], pDst, cnt * sizeof(q15_t) * g_channel);
 
    result[pos].buffer = (void*)MP.Virt2Phys(&out_buffer[pos][0]);
    result[pos].sample = cnt;

    ret = MP.Send(sndid, &result[pos], 0);
    pos = (pos + 1) % g_result_size;

    if (ret < 0) {
      errorLoop(10);
    }
  }
}

void errorLoop(int num)
{
  int i;

  while (1) {
    for (i = 0; i < num; i++) {
      ledOn(LED0);
      delay(300);
      ledOff(LED0);
      delay(300);
    }
    delay(1000);
  }
}
