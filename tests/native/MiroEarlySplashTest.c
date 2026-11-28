// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Uefi.h>
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

static UINT32 *mPixels;
#define FRAME_BASE ((UINTN)mPixels)
#include "../../uefi/handoff/bootselect/EarlySplash.c"

int PianoEarlySplashAllowed(const void *Fdt) { return Fdt != NULL; }

static void Save(const char *Path) {
  FILE *File=fopen(Path,"wb");assert(File);
  fprintf(File,"P6\n%u %u\n255\n",FRAME_WIDTH,FRAME_HEIGHT);
  for(UINTN I=0;I<(UINTN)FRAME_WIDTH*FRAME_HEIGHT;++I) {
    unsigned char Rgb[3]={(unsigned char)(mPixels[I]>>16),
      (unsigned char)(mPixels[I]>>8),(unsigned char)mPixels[I]};
    assert(fwrite(Rgb,1,3,File)==3);
  }
  assert(fclose(File)==0);
}

int main(int Argc,char **Argv) {
  assert(Argc==3);
  _Static_assert(FRAME_WIDTH==1440 && FRAME_HEIGHT==3200,"miro scanout geometry");
  UINTN Bytes=(UINTN)FRAME_WIDTH*FRAME_HEIGHT*4;
  UINTN Page=(UINTN)sysconf(_SC_PAGESIZE);
  assert(Bytes%Page==0);
  unsigned char *Map=mmap(NULL,Bytes+2*Page,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  assert(Map!=MAP_FAILED);
  mPixels=(UINT32 *)(Map+Page);
  assert(mprotect(mPixels,Bytes,PROT_READ|PROT_WRITE)==0);
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Red={0,0,255,0};
  assert(Fill(NULL,&Red,EfiBltVideoFill,0,0,1439,3199,1,1,0)==EFI_SUCCESS);
  assert(mPixels[1440*3200-1]==0xFF0000);
  assert(Fill(NULL,&Red,EfiBltVideoFill,0,0,1440,0,1,1,0)==EFI_INVALID_PARAMETER);
  assert(Fill(NULL,&Red,EfiBltVideoFill,0,0,0,3200,1,1,0)==EFI_INVALID_PARAMETER);
  assert(Fill(NULL,&Red,EfiBltVideoFill,0,0,1439,3199,2,1,0)==EFI_INVALID_PARAMETER);
  for(unsigned Target=0;Target<2;++Target) {
    for(unsigned Seconds=0;Seconds<=3;++Seconds) {
      PianoEarlyDrawChoice(Target,Seconds,1,1);
      UINT32 Y=FRAME_HEIGHT*(55+Target*10)/100;
      assert(mPixels[Y*FRAME_WIDTH+FRAME_WIDTH/10]==0x4273E2);
      assert(mPixels[FRAME_WIDTH*FRAME_HEIGHT-1]==0xFFFFFF);
    }
    PianoEarlyDrawChoice(Target,3,1,1);
    Save(Argv[Target+1]);
    PianoEarlyDrawChoice(Target,2,0,0);
    ChoiceMessage(Target?"STARTING UEFI":"STARTING ANDROID");
  }
  assert(munmap(Map,Bytes+2*Page)==0);
  puts("miro picker: 1440x3200 stride, guarded drawing and both selections passed");
  return 0;
}
