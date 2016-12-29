/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

/* Modified by Luna authors to remove unused code */

#include "DES.h"

void DES_set_key(const DES_cblock *key, DES_key_schedule *schedule) {
  static const int shifts2[16] = {0, 0, 1, 1, 1, 1, 1, 1,
                                  0, 1, 1, 1, 1, 1, 1, 0};
  uint32_t c, d, t, s, t2;
  const uint8_t *in;
  int i;

  in = key->bytes;

  c2l(in, c);
  c2l(in, d);

  /* do PC1 in 47 simple operations :-)
   * Thanks to John Fletcher (john_fletcher@lccmail.ocf.llnl.gov)
   * for the inspiration. :-) */
  PERM_OP(d, c, t, 4, 0x0f0f0f0fL);
  HPERM_OP(c, t, -2, 0xcccc0000L);
  HPERM_OP(d, t, -2, 0xcccc0000L);
  PERM_OP(d, c, t, 1, 0x55555555L);
  PERM_OP(c, d, t, 8, 0x00ff00ffL);
  PERM_OP(d, c, t, 1, 0x55555555L);
  d = (((d & 0x000000ffL) << 16L) | (d & 0x0000ff00L) |
       ((d & 0x00ff0000L) >> 16L) | ((c & 0xf0000000L) >> 4L));
  c &= 0x0fffffffL;

  for (i = 0; i < ITERATIONS; i++) {
    if (shifts2[i]) {
      c = ((c >> 2L) | (c << 26L));
      d = ((d >> 2L) | (d << 26L));
    } else {
      c = ((c >> 1L) | (c << 27L));
      d = ((d >> 1L) | (d << 27L));
    }
    c &= 0x0fffffffL;
    d &= 0x0fffffffL;
    /* could be a few less shifts but I am to lazy at this
     * point in time to investigate */
    s = des_skb[0][(c) & 0x3f] |
        des_skb[1][((c >> 6L) & 0x03) | ((c >> 7L) & 0x3c)] |
        des_skb[2][((c >> 13L) & 0x0f) | ((c >> 14L) & 0x30)] |
        des_skb[3][((c >> 20L) & 0x01) | ((c >> 21L) & 0x06) |
                   ((c >> 22L) & 0x38)];
    t = des_skb[4][(d) & 0x3f] |
        des_skb[5][((d >> 7L) & 0x03) | ((d >> 8L) & 0x3c)] |
        des_skb[6][(d >> 15L) & 0x3f] |
        des_skb[7][((d >> 21L) & 0x0f) | ((d >> 22L) & 0x30)];

    /* table contained 0213 4657 */
    t2 = ((t << 16L) | (s & 0x0000ffffL)) & 0xffffffffL;
    schedule->subkeys[i][0] = ROTATE(t2, 30) & 0xffffffffL;

    t2 = ((s >> 16L) | (t & 0xffff0000L));
    schedule->subkeys[i][1] = ROTATE(t2, 26) & 0xffffffffL;
  }
}

static void DES_encrypt2(uint32_t *data, const DES_key_schedule *ks, int enc) {
  uint32_t l, r, t, u;

  r = data[0];
  l = data[1];

  /* Things have been modified so that the initial rotate is done outside the
   * loop.  This required the DES_SPtrans values in sp.h to be rotated 1 bit to
   * the right. One perl script later and things have a 5% speed up on a
   * sparc2. Thanks to Richard Outerbridge <71755.204@CompuServe.COM> for
   * pointing this out. */
  /* clear the top bits on machines with 8byte longs */
  r = ROTATE(r, 29) & 0xffffffffL;
  l = ROTATE(l, 29) & 0xffffffffL;

  /* I don't know if it is worth the effort of loop unrolling the
   * inner loop */
  if (enc) {
    D_ENCRYPT(ks, l, r, 0);
    D_ENCRYPT(ks, r, l, 1);
    D_ENCRYPT(ks, l, r, 2);
    D_ENCRYPT(ks, r, l, 3);
    D_ENCRYPT(ks, l, r, 4);
    D_ENCRYPT(ks, r, l, 5);
    D_ENCRYPT(ks, l, r, 6);
    D_ENCRYPT(ks, r, l, 7);
    D_ENCRYPT(ks, l, r, 8);
    D_ENCRYPT(ks, r, l, 9);
    D_ENCRYPT(ks, l, r, 10);
    D_ENCRYPT(ks, r, l, 11);
    D_ENCRYPT(ks, l, r, 12);
    D_ENCRYPT(ks, r, l, 13);
    D_ENCRYPT(ks, l, r, 14);
    D_ENCRYPT(ks, r, l, 15);
  } else {
    D_ENCRYPT(ks, l, r, 15);
    D_ENCRYPT(ks, r, l, 14);
    D_ENCRYPT(ks, l, r, 13);
    D_ENCRYPT(ks, r, l, 12);
    D_ENCRYPT(ks, l, r, 11);
    D_ENCRYPT(ks, r, l, 10);
    D_ENCRYPT(ks, l, r, 9);
    D_ENCRYPT(ks, r, l, 8);
    D_ENCRYPT(ks, l, r, 7);
    D_ENCRYPT(ks, r, l, 6);
    D_ENCRYPT(ks, l, r, 5);
    D_ENCRYPT(ks, r, l, 4);
    D_ENCRYPT(ks, l, r, 3);
    D_ENCRYPT(ks, r, l, 2);
    D_ENCRYPT(ks, l, r, 1);
    D_ENCRYPT(ks, r, l, 0);
  }
  /* rotate and clear the top bits on machines with 8byte longs */
  data[0] = ROTATE(l, 3) & 0xffffffffL;
  data[1] = ROTATE(r, 3) & 0xffffffffL;
}

void DES_encrypt3(uint32_t *data, const DES_key_schedule *ks1,
                  const DES_key_schedule *ks2, const DES_key_schedule *ks3) {
  uint32_t l, r;

  l = data[0];
  r = data[1];
  IP(l, r);
  data[0] = l;
  data[1] = r;
  DES_encrypt2((uint32_t *)data, ks1, DES_ENCRYPT);
  DES_encrypt2((uint32_t *)data, ks2, DES_DECRYPT);
  DES_encrypt2((uint32_t *)data, ks3, DES_ENCRYPT);
  l = data[0];
  r = data[1];
  FP(r, l);
  data[0] = l;
  data[1] = r;
}

void DES_decrypt3(uint32_t *data, const DES_key_schedule *ks1,
                  const DES_key_schedule *ks2, const DES_key_schedule *ks3) {
  uint32_t l, r;

  l = data[0];
  r = data[1];
  IP(l, r);
  data[0] = l;
  data[1] = r;
  DES_encrypt2((uint32_t *)data, ks3, DES_DECRYPT);
  DES_encrypt2((uint32_t *)data, ks2, DES_ENCRYPT);
  DES_encrypt2((uint32_t *)data, ks1, DES_DECRYPT);
  l = data[0];
  r = data[1];
  FP(r, l);
  data[0] = l;
  data[1] = r;
}

void DES_ecb3_encrypt(const DES_cblock *input, DES_cblock *output,
                      const DES_key_schedule *ks1, const DES_key_schedule *ks2,
                      const DES_key_schedule *ks3, int enc) {
  uint32_t l0, l1;
  uint32_t ll[2];
  const uint8_t *in = input->bytes;
  uint8_t *out = output->bytes;

  c2l(in, l0);
  c2l(in, l1);
  ll[0] = l0;
  ll[1] = l1;
  if (enc) {
    DES_encrypt3(ll, ks1, ks2, ks3);
  } else {
    DES_decrypt3(ll, ks1, ks2, ks3);
  }
  l0 = ll[0];
  l1 = ll[1];
  l2c(l0, out);
  l2c(l1, out);
}
