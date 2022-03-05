#ifndef UTIL_HPP
#define UTIL_HPP

#include <windows.h>

#include <string>
#include <cstdint>
#include <iostream>
#include <assert.h>

uint64_t BTOL(const uint8_t *msb, uint8_t size)
{
   uint8_t be[8] = { 0 };

   for (int i = 0; i < size; i++)
   {
     be[i] = msb[i];
   }

   for (int i = 0; i < size/2; i++)
   {
      be[i]        =  be[i] ^ be[size-1-i];
      be[size-1-i] =  be[i] ^ be[size-1-i];
      be[i]        =  be[i] ^ be[size-1-i];
   }

   if (size == 8) 
   {
     return (*((uint64_t *)&be));
   }
   else if (size == 4)
   {
     return (*((uint32_t *)&be));
   }
   else if (size == 2)
   {
     return (*((uint16_t *)&be));
   }
   else
   {
     assert(false);
     return 0;
   }
}
uint16_t BTOL16(const uint8_t *msb)
{
  return static_cast<uint16_t>(BTOL(msb, 2));
}
uint32_t BTOL32(const uint8_t *msb)
{
  return static_cast<uint32_t>(BTOL(msb, 4));
}
uint64_t BTOL64(const uint8_t *msb)
{
  return BTOL(msb, 8);
}

void LTOB(uint64_t value, unsigned char *memory, int size)
{
  for (int i = 0; i < size; i++)
  {
    memory[size - 1 - i] = *((unsigned char *)(&value) + i);
  }
}
void LTOB16(uint64_t value, unsigned char *memory)
{
  LTOB(value, memory, 2);
}
void LTOB32(uint64_t value, unsigned char *memory)
{
  LTOB(value, memory, 4);
}
void LTOB64(uint64_t value, unsigned char *memory)
{
  LTOB(value, memory, 8);
}

void DumpData(const std::string& file, const uint8_t *b, uint64_t l)
{
  FILE * pFile;
  fopen_s(&pFile, file.c_str(), "wb");
  fwrite (b, 1, l, pFile);
  fclose (pFile);
}

#endif