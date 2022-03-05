#include <windows.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <stdio.h>

unsigned char block1[2*1024*1024];
unsigned char block2[2*1024*1024];

int main(int argc, char *argv[])
{
  HANDLE hFirst, hSecond;
  DWORD BytesReturned;
  BOOL fRet;

  if (argc < 3 || !argv[1] || !argv[2] || !argv[3] || !argv[4])
  {
	wprintf(L"\n Blockdiff.exe [device1] [device2] [blocksize] [count]\n");
	return 0;
  }
  /** 
   * open the first block device
   */
  hFirst = CreateFileA(
             argv[1],
             GENERIC_READ|GENERIC_WRITE,
             FILE_SHARE_READ|FILE_SHARE_WRITE,
             NULL,
             OPEN_EXISTING,
             FILE_FLAG_NO_BUFFERING,
             NULL);

  if (hFirst == INVALID_HANDLE_VALUE) 
  {
    printf("\n Unable to open handle to source block device [%s]. Error : %d\n", argv[1], GetLastError());
    goto _end;
  }
  /** 
   * open the second block device
   */
  hSecond = CreateFileA(
              argv[2],
              GENERIC_READ|GENERIC_WRITE,
              FILE_SHARE_READ|FILE_SHARE_WRITE,
              NULL,
              OPEN_EXISTING,
              FILE_FLAG_NO_BUFFERING,
              NULL);

  if (hSecond == INVALID_HANDLE_VALUE)
  {
    printf("\n Unable to open handle to target block device [%s]. Error : %d\n", argv[2], GetLastError());
    goto _end;
  }

  fRet = DeviceIoControl(
           hFirst,
           FSCTL_ALLOW_EXTENDED_DASD_IO,
           NULL,
           0,
           NULL,
           0,
           &BytesReturned,
           NULL);

  if (!fRet)
  {
    printf("\n FSCTL_ALLOW_EXTENDED_DASD_IO failed for [%s]. Error : %d\n", argv[1], GetLastError());
    goto _end;
  }

  fRet = DeviceIoControl(
           hSecond,
           FSCTL_ALLOW_EXTENDED_DASD_IO,
           NULL,
           0,
           NULL,
           0,
           &BytesReturned,
           NULL);

  if (!fRet)
  {
    printf("\n FSCTL_ALLOW_EXTENDED_DASD_IO failed for [%s]. Error : %d\n", argv[2], GetLastError());
    goto _end;
  }

  LARGE_INTEGER offset;

  int nBlocks = std::atoll(argv[4]);
  int nBlockSize = std::atoll(argv[3]);

  for (uint64_t i = 0; i <= nBlocks; i++)
  {
    offset.QuadPart = i * nBlockSize;

    fRet = SetFilePointerEx(hFirst, offset, NULL, FILE_BEGIN);
	  if (fRet == 0)
    {
      printf("SetFilePointerEx failed on first device, error %d", GetLastError());
	    goto _end;
  	}

    fRet = SetFilePointerEx(hSecond, offset, NULL, FILE_BEGIN);
  	if (fRet == 0)
    {
      printf("SetFilePointerEx failed on second device, error %d", GetLastError());
	    goto _end;
	  }

    fRet = ReadFile(hFirst, block1, nBlockSize, &BytesReturned, NULL);
	  if (!fRet)
    {
      printf("ReadFile failed on first device, error %d", GetLastError());
	    goto _end;
	  }

    fRet = ReadFile(hSecond, block2, nBlockSize, &BytesReturned, NULL);
	  if (!fRet)
    {
      printf("ReadFile failed on second device, error %d", GetLastError());
	    goto _end;
	  }

	  if (memcmp(block1, block2, nBlockSize))
	  {
      wprintf(L"\n block1 %I64d at offset %I64d : \n\n", i, offset.QuadPart);
   
      for(int i = 0; i < 32; i++)
      {
	      for (int j = 0; j < 16; j++)
	      {
	        printf(" %.2x ", block1[(i*16) + j]);
	      }
	      printf("    ");
	      for (int j = 0; j < 16; j++)
	      {
	        if (isprint(block1[(i*16) + j]))
	          printf("%c", block1[(i*16) + j]);
	        else 
		        printf(".");
	      }
	      printf("\n");
      }

	    wprintf(L"\n block2 %I64d at offset %I64d : \n\n", i, offset.QuadPart);
   
      for(int i = 0; i < 32; i++)
      {
	      for (int j = 0; j < 16; j++)
	      {
	        printf(" %.2x ", block2[(i*16) + j]);
	      }
	      printf("    ");
	      for (int j = 0; j < 16; j++)
	      {
	        if (isprint(block2[(i*16) + j]))
	          printf("%c", block2[(i*16) + j]);
	        else 
		        printf(".");
	      }
	      printf("\n");
      }
	  }
  }

 _end:

 return 0;
}
