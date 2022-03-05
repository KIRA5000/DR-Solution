#include <windows.h>
#include <string>
#include <cstdlib>
#include <stdio.h>
  
int main(int argc, char *argv[])
{
  long long nsectors;
  DWORD bytesReturned;
  HANDLE hSource;
  BOOL bOk;

  if (argc < 4)
  {
    wprintf(L"\n Dumpsectors.exe [device] [start sector] [number of sectors]\n");
    return 0;
  }
  /* 
   * open the source block device
   */
  hSource = CreateFileA(
              argv[1],
              GENERIC_READ|GENERIC_WRITE,
              FILE_SHARE_READ|FILE_SHARE_WRITE,
              NULL,
              OPEN_EXISTING,
              FILE_FLAG_NO_BUFFERING,
              NULL);

  if (hSource == INVALID_HANDLE_VALUE) 
  {
    printf("\n Unable to open handle to source block device [%s]. Error : %d\n", argv[1], GetLastError());
    goto _end;
  }

  bOk = DeviceIoControl(
          hSource,
          FSCTL_ALLOW_EXTENDED_DASD_IO,
          NULL,
          0,
          NULL,
          0,
          &bytesReturned,
          NULL);

  if (!bOk) 
  {
    printf("\n FSCTL_ALLOW_EXTENDED_DASD_IO failed for [%s]. Error : %d\n", argv[1], GetLastError());
  }

  LARGE_INTEGER offset;
  BYTE sector[512];

  nsectors = std::atoll(argv[3]);

  for (int n = 0; n < nsectors; n++)
  {
    memset(sector, 0, 512);

    offset.QuadPart = (_atoi64(argv[2]) + n) * 512;
   
    if (SetFilePointerEx(hSource, offset, NULL, FILE_BEGIN))
    {
      if (!ReadFile(hSource, sector, 512, &bytesReturned, NULL))
      {
        printf("Failed to read the sector, error %d\n", GetLastError());
        goto _end;
      }
    }
    else
    {
      printf("SetFilePointerEx failed. Error %d\n", GetLastError());
      goto _end;
    }

    wprintf(L"\n Sector %d at offset %I64d : \n\n", n, offset.QuadPart);
   
    for (int i = 0; i < 32; i++)
    {
        for (int j = 0; j < 16; j++)
        {
          printf(" %.2x ", sector[(i*16) + j]);
        }
        printf("    ");
        for (int j = 0; j < 16; j++)
        {
          if (isprint(sector[(i*16) + j]))
            printf("%c", sector[(i*16) + j]);
          else 
            printf(".");
        }
        printf("\n");
    }
  }
   
  _end:

  return 0;
}