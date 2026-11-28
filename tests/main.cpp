#include <cstdio>

#include "check.h"

void testAspectFit();
void testAudioMixer();
void testDelayLine();
void testEffectChain();
void testMicFeed();
void testNv12();
void testQualityPolicy();
void testRecovery();
void testResampler();
void testRole();
void testRouteMatrix();
void testSdkToken();
void testSlotOutput();
void testTestTone();
void testUpdatePolicy();
void testYuv();

int main() {
  testAspectFit();
  testAudioMixer();
  testDelayLine();
  testEffectChain();
  testMicFeed();
  testNv12();
  testQualityPolicy();
  testRecovery();
  testResampler();
  testRole();
  testRouteMatrix();
  testSdkToken();
  testSlotOutput();
  testTestTone();
  testUpdatePolicy();
  testYuv();
  if (testing::failures() == 0) {
    std::printf("all tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}
