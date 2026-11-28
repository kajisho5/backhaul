#include <cstdio>

#include "check.h"

void testAspectFit();
void testAudioMixer();
void testDelayLine();
void testEffectChain();
void testLineQuality();
void testMicFeed();
void testNv12();
void testPreshowCheck();
void testQualityPolicy();
void testRecovery();
void testResampler();
void testRole();
void testRouteMatrix();
void testSdkToken();
void testShowLog();
void testSlotOutput();
void testTestTone();
void testUpdatePolicy();
void testWatcher();
void testYuv();

int main() {
  testAspectFit();
  testAudioMixer();
  testDelayLine();
  testEffectChain();
  testLineQuality();
  testMicFeed();
  testNv12();
  testPreshowCheck();
  testQualityPolicy();
  testRecovery();
  testResampler();
  testRole();
  testRouteMatrix();
  testSdkToken();
  testShowLog();
  testSlotOutput();
  testTestTone();
  testUpdatePolicy();
  testWatcher();
  testYuv();
  if (testing::failures() == 0) {
    std::printf("all tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}
