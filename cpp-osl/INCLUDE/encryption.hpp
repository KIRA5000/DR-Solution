#ifndef ENCRYPTION_HPP
#define ENCRYPTION_HPP

#include <openssl/evp.h>

int Base64Encode(unsigned char *encoded, const unsigned char *source, int n)
{
  return EVP_EncodeBlock(encoded, source, n);
}

int MessageDigest(const unsigned char *message, size_t message_len, unsigned char *digest, unsigned int *digest_len)
{
  int fRet = 0;

  EVP_MD_CTX *mdctx = NULL;

  mdctx = EVP_MD_CTX_create();

  if (mdctx == NULL)
    goto _end;

  fRet = EVP_DigestInit_ex(mdctx, EVP_sha1(), NULL);

  if (1 != fRet)
    goto _end;

  fRet = EVP_DigestUpdate(mdctx, message, message_len);

  if (1 != fRet)
    goto _end;

  fRet = EVP_DigestFinal_ex(mdctx, digest, digest_len);

  if (1 != fRet)
    goto _end;

  _end:

  if (mdctx)
  {
    EVP_MD_CTX_destroy(mdctx);
  }

  return fRet;
}

#endif