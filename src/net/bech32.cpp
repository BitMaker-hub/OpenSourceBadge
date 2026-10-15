#include "bech32.h"

// Implementación mínima del BIP-173 (bech32), solo lo que necesita Nostr
namespace bech32 {

static const char* CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

static uint32_t polymod(const uint8_t* values, size_t len) {
  static const uint32_t GEN[5] = {0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3};
  uint32_t chk = 1;
  for (size_t i = 0; i < len; i++) {
    const uint8_t top = chk >> 25;
    chk = (chk & 0x1ffffff) << 5 ^ values[i];
    for (int j = 0; j < 5; j++) if ((top >> j) & 1) chk ^= GEN[j];
  }
  return chk;
}

// Los valores para la suma de control: el prefijo ("npub") expandido y luego los datos
static size_t withPrefix(const char* hrp, const uint8_t* data, size_t len, uint8_t* out) {
  const size_t h = strlen(hrp);
  size_t n = 0;
  for (size_t i = 0; i < h; i++) out[n++] = hrp[i] >> 5;
  out[n++] = 0;
  for (size_t i = 0; i < h; i++) out[n++] = hrp[i] & 31;
  for (size_t i = 0; i < len; i++) out[n++] = data[i];
  return n;
}

// Reagrupa bits: de grupos de `from` bits a grupos de `to` bits
static bool regroup(const uint8_t* in, size_t inLen, int from, int to, bool pad, uint8_t* out, size_t& outLen) {
  uint32_t acc = 0;
  int bits = 0;
  outLen = 0;
  for (size_t i = 0; i < inLen; i++) {
    acc = acc << from | in[i];
    bits += from;
    while (bits >= to) {
      bits -= to;
      out[outLen++] = (acc >> bits) & ((1 << to) - 1);
    }
  }
  if (pad && bits) out[outLen++] = (acc << (to - bits)) & ((1 << to) - 1);
  else if (!pad && (bits >= from || ((acc << (to - bits)) & ((1 << to) - 1)))) return false;
  return true;
}

String npubToHex(const String& npubIn) {
  String npub = npubIn;
  npub.trim();
  npub.toLowerCase();
  if (!npub.startsWith("npub1") || npub.length() != 63) return "";

  // Letras -> valores de 5 bits (las 6 últimas son la suma de control)
  uint8_t values[58];
  for (int i = 0; i < 58; i++) {
    const char* p = strchr(CHARSET, npub[5 + i]);
    if (!p) return "";
    values[i] = p - CHARSET;
  }
  uint8_t check[4 + 1 + 4 + 58];
  if (polymod(check, withPrefix("npub", values, 58, check)) != 1) return "";

  uint8_t key[40];
  size_t keyLen;
  if (!regroup(values, 52, 5, 8, false, key, keyLen) || keyLen != 32) return "";
  String hex;
  char b[3];
  for (int i = 0; i < 32; i++) { snprintf(b, sizeof b, "%02x", key[i]); hex += b; }
  return hex;
}

String hexToNpub(const String& hex) {
  if (hex.length() != 64) return "";
  uint8_t key[32];
  for (int i = 0; i < 32; i++) key[i] = strtoul(hex.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);

  uint8_t values[52 + 6];
  size_t n;
  regroup(key, 32, 8, 5, true, values, n);

  // Suma de control: se calcula con seis ceros al final y se reparte en 6 letras
  uint8_t check[4 + 1 + 4 + 58];
  size_t len = withPrefix("npub", values, n, check);
  for (int i = 0; i < 6; i++) check[len++] = 0;
  const uint32_t mod = polymod(check, len) ^ 1;
  for (int i = 0; i < 6; i++) values[n + i] = (mod >> (5 * (5 - i))) & 31;

  String out = "npub1";
  for (size_t i = 0; i < n + 6; i++) out += CHARSET[values[i]];
  return out;
}

}  // namespace bech32
