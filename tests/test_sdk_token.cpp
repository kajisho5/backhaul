#include <string>

#include "check.h"
#include "core/sdk_token.h"
#include "core/sha256.h"

namespace {

std::string hex(const backhaul::Sha256Digest& d) {
  static const char* digits = "0123456789abcdef";
  std::string s;
  for (const auto b : d) {
    s.push_back(digits[b >> 4]);
    s.push_back(digits[b & 15]);
  }
  return s;
}

}  // namespace

void testSdkToken() {
  using namespace backhaul;

  // FIPS 180-4 examples.
  CHECK(hex(sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(hex(sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(hex(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  // One million 'a': crosses many block boundaries.
  CHECK(hex(sha256(std::string(1000000, 'a'))) ==
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");

  // RFC 4231 test cases 1, 2 and 6 (key longer than one block).
  CHECK(hex(hmacSha256(std::string(20, '\x0b'), "Hi There")) ==
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
  CHECK(hex(hmacSha256("Jefe", "what do ya want for nothing?")) ==
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
  CHECK(hex(hmacSha256(std::string(131, '\xaa'),
                       "Test Using Larger Than Block-Size Key - Hash Key First")) ==
        "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");

  // RFC 4648 vectors, without padding.
  CHECK(base64Url("") == "");
  CHECK(base64Url("f") == "Zg");
  CHECK(base64Url("fo") == "Zm8");
  CHECK(base64Url("foo") == "Zm9v");
  CHECK(base64Url("foob") == "Zm9vYg");
  CHECK(base64Url("fooba") == "Zm9vYmE");
  CHECK(base64Url("foobar") == "Zm9vYmFy");
  const unsigned char high[] = {0xfb, 0xff, 0xfe};
  CHECK(base64Url(high, 3) == "-__-");

  // RFC 7515 appendix A.1 signs with HS256; same header/payload bytes and key give the
  // same signature, which proves the "header.payload" signing step.
  {
    const std::string signedPart =
        "eyJ0eXAiOiJKV1QiLA0KICJhbGciOiJIUzI1NiJ9."
        "eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQogImh0dHA6Ly9leGFtcGxlLmNvbS9pc19yb290Ijp0cnVlfQ";
    const unsigned char key[] = {
        3,   35,  53,  75,  43,  15,  165, 188, 131, 126, 6,   101, 119, 123, 166, 143,
        90,  179, 40,  230, 240, 84,  201, 40,  169, 15,  132, 178, 210, 80,  46,  191,
        211, 251, 90,  146, 210, 6,   71,  239, 150, 138, 180, 195, 119, 98,  61,  34,
        61,  46,  33,  114, 5,   46,  79,  8,   192, 205, 154, 245, 103, 208, 128, 163};
    const std::string k(reinterpret_cast<const char*>(key), sizeof(key));
    const Sha256Digest mac = hmacSha256(k, signedPart);
    CHECK(base64Url(mac.data(), mac.size()) == "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");
  }

  // Token layout with made-up credentials.
  {
    const std::string token = makeSdkToken("SampleClientId_01", "sample-secret", 1700000000, 7200);
    const std::string header = base64Url("{\"alg\":\"HS256\",\"typ\":\"JWT\"}");
    const std::string payload = base64Url(
        "{\"appKey\":\"SampleClientId_01\",\"iat\":1700000000,\"exp\":1700007200,"
        "\"tokenExp\":1700007200}");
    const std::string signedPart = header + "." + payload;
    const Sha256Digest mac = hmacSha256("sample-secret", signedPart);
    CHECK(token == signedPart + "." + base64Url(mac.data(), mac.size()));
    CHECK(token != makeSdkToken("SampleClientId_01", "other-secret", 1700000000, 7200));
  }

  // Inputs that cannot give a valid token.
  CHECK(makeSdkToken("", "s", 1700000000, 7200).empty());
  CHECK(makeSdkToken("id", "", 1700000000, 7200).empty());
  CHECK(makeSdkToken("bad\"id", "s", 1700000000, 7200).empty());
  CHECK(makeSdkToken("id", "s", -1, 7200).empty());
  CHECK(makeSdkToken("id", "s", 1700000000, 1799).empty());
  CHECK(makeSdkToken("id", "s", 1700000000, 48 * 3600 + 1).empty());
  CHECK(!makeSdkToken("id", "s", 1700000000, 1800).empty());
  CHECK(!makeSdkToken("id", "s", 1700000000, 48 * 3600).empty());
}
