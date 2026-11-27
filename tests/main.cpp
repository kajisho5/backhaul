#include <cstdio>

#include "check.h"

void testAspectFit();
void testAudioMixer();
void testDelayLine();
void testQualityPolicy();
void testResampler();
void testRole();
void testRouteMatrix();
void testSdkToken();
void testSlotOutput();
void testYuv();

int main() {
  testAspectFit();
  testAudioMixer();
  testDelayLine();
  testQualityPolicy();
  testResampler();
  testRole();
  testRouteMatrix();
  testSdkToken();
  testSlotOutput();
  testYuv();
  if (testing::failures() == 0) {
    std::printf("all tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}
