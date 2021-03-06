#include "hash.hpp"

extern "C"
{
#include "sha256.h"
}

std::string sha256(const std::string& str)
{
	sha256_context ctx;
	char digest[32];
	char cdigest[64];

	sha256_start(&ctx);
	sha256_update(&ctx, str.c_str(), str.length());
	sha256_finish(&ctx, digest);

	for (int i = 0; i < 32; ++i)
	{
		cdigest[i*2]   = "0123456789abcdef"[((digest[i] >> 4) & 0x0F)];
		cdigest[i*2+1] = "0123456789abcdef"[((digest[i]) & 0x0F)];
	}

	return std::string(cdigest, 64);
}
