// SPDX-License-Identifier: BSD-2-Clause-Patent
// Miro FocalTech FT3683 SPI touchscreen over bit-banged TLMM GPIO SPI.
// Restores all mux, output, AVDD, reset and IRQ latch state on ExitBootServices.
#include "MiroTouch.h"
#include <Protocol/AbsolutePointer.h>
#include <Protocol/DevicePath.h>
#include <Guid/ConsoleInDevice.h>
#include <Guid/EventGroup.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/FdtLib.h>

#define TLMM_BASE     0x0F100000U
#define PIN_CTL(P)    (TLMM_BASE + (P) * 0x1000U)
#define PIN_IO(P)     (PIN_CTL(P) + 4U)
#define IRQ_CFG(P)    (PIN_CTL(P) + 8U)
#define IRQ_STATUS(P) (PIN_CTL(P) + 12U)

#define PIN_MISO      40U
#define PIN_MOSI      41U
#define PIN_CLK       42U
#define PIN_CS        43U
#define PIN_AVDD      114U
#define PIN_RESET     161U
#define PIN_IRQ       162U

#define TOUCH_NODE    "/soc/qcom,qupv3_1_geni_se@ac0000/spi@a88000/focaltech_ts@0"
#define MAX_X         1439U
#define MAX_Y         3199U
#define QUEUE_LEN     32U
#define PKT_BYTES     14U

STATIC CONST UINT32 mSpiPins[4] = {PIN_MISO, PIN_MOSI, PIN_CLK, PIN_CS};
STATIC UINT32 mSavedCtl[4], mSavedIo[4];
STATIC UINT32 mAvddCtl, mAvddIo, mRstCtl, mRstIo;
STATIC UINT32 mIrqCtl, mIrqCfg;
STATIC BOOLEAN mPinsSaved, mPowerSaved, mIrqOwned, mRunning, mRetained, mExited, mBusy;
STATIC BOOLEAN mPressed;
STATIC UINT8 mChipIdH, mChipIdL, mLastBuf[4], mLastRx[4];
STATIC UINT32 mFrames, mEvents, mTransfers, mBad;
STATIC EFI_STATUS mStatus = EFI_NOT_STARTED;
STATIC EFI_STATUS mRailStatus = EFI_NOT_STARTED;
STATIC UINT32 mRailStep, mRailBase;

typedef struct {
  UINT64 Revision;
  VOID *Init;
  EFI_STATUS (EFIAPI *ValidDrv)(UINT32, BOOLEAN *);
  VOID *TcsCommands, *CreateInternal, *CreateExplicit;
  EFI_STATUS (EFIAPI *Create)(UINT32, CONST CHAR8 *, VOID **);
  VOID *Invalidate, *RegisterCallback;
  EFI_STATUS (EFIAPI *Delete)(VOID *);
  EFI_STATUS (EFIAPI *Find)(UINT32, VOID **);
  EFI_STATUS (EFIAPI *Churn)(VOID *, UINT32);
  VOID *Unused[7];
  EFI_STATUS (EFIAPI *Issue)(VOID *, UINT32, BOOLEAN, UINT32, UINT32, UINT32 *);
} MIRO_RPMH;
typedef struct {
  UINT64 Revision;
  VOID *Query;
  EFI_STATUS (EFIAPI *Address)(VOID *, CONST CHAR8 *, UINT32 *);
} MIRO_CMD_DB;
typedef struct {
  UINT32 Data;
  BOOLEAN Valid;
  UINT8 Reserved[3];
  UINT32 Dirty;
} MIRO_RPMH_VOTE;
typedef struct _MIRO_RPMH_DRV {
  UINT32 Drv, State, Priority;
  MIRO_RPMH_VOTE AtRpmh, Local[3];
  struct _MIRO_RPMH_DRV *Next;
} MIRO_RPMH_DRV;
typedef struct {
  UINT32 Address;
  MIRO_RPMH_DRV *Votes;
  VOID *PdcVotes;
} MIRO_RPMH_RESOURCE;
STATIC_ASSERT (OFFSET_OF (MIRO_RPMH, Issue) == 0x98, "Miro RPMh issue ABI");
STATIC_ASSERT (OFFSET_OF (MIRO_RPMH_DRV, Local) == 0x18 && OFFSET_OF (MIRO_RPMH_DRV, Next) == 0x40, "Miro RPMh vote ABI");

STATIC MIRO_RPMH *mRpmh;
STATIC VOID *mRpmhHandle;
STATIC struct {UINT32 Address, Before, After; BOOLEAN Changed;} mLdo12[3];

STATIC EFI_ABSOLUTE_POINTER_PROTOCOL mPointer;
STATIC EFI_ABSOLUTE_POINTER_MODE mMode = {0, 0, 0, 65535, 65535, 0, 0};
STATIC EFI_ABSOLUTE_POINTER_STATE mQueue[QUEUE_LEN], mLast;
STATIC UINTN mHead, mTail;
STATIC EFI_HANDLE mHandle;
STATIC EFI_EVENT mTimer, mExit;

STATIC struct {
  VENDOR_DEVICE_PATH Vendor;
  EFI_DEVICE_PATH_PROTOCOL End;
} mPath = {
  {{HARDWARE_DEVICE_PATH, HW_VENDOR_DP, {sizeof(VENDOR_DEVICE_PATH), 0}},
   {0x7A4E11D3, 0x8F2B, 0x4C91, {0x9A, 0x12, 0x6D, 0x3F, 0x80, 0x45, 0x72, 0x19}}},
  {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {4, 0}}
};

STATIC VOID Drive (UINT32 Pin, BOOLEAN High) {
  MmioWrite32 (PIN_IO (Pin), High ? 2U : 0U);
}

STATIC UINT32 GpioMuxConfig (UINT32 OrigCtl, BOOLEAN Output, UINT32 Pull) {
  UINT32 Value = (OrigCtl & ~0x3FFU) | (2U << 6) | Pull | (Output ? BIT9 : 0U);
  if (Value & BIT11) {
    Value |= BIT12;
  }
  return Value;
}

STATIC UINT8 SpiByte (UINT8 Tx) {
  UINT8 Rx = 0;
  for (UINTN I = 0; I < 8; ++I) {
    Drive (PIN_MOSI, (Tx & 0x80U) != 0);
    Tx <<= 1;
    NanoSecondDelay (500);
    Drive (PIN_CLK, TRUE);
    Rx = (UINT8)((Rx << 1) | (MmioRead32 (PIN_IO (PIN_MISO)) & 1U));
    NanoSecondDelay (500);
    Drive (PIN_CLK, FALSE);
  }
  return Rx;
}

STATIC UINT16 FtsCrc16 (CONST UINT8 *Data, UINTN Len) {
  UINT16 Crc = 0xFFFF;
  for (UINTN I = 0; I < Len; ++I) {
    Crc ^= Data[I];
    for (UINTN B = 0; B < 8; ++B) {
      if (Crc & 1U) {
        Crc = (Crc >> 1) ^ 0x8408U;
      } else {
        Crc >>= 1;
      }
    }
  }
  return Crc;
}

STATIC EFI_STATUS FtsRead (UINT8 Addr, UINT8 *Out, UINTN Len) {
  if (Out == NULL || Len == 0 || Len > 128) {
    return EFI_INVALID_PARAMETER;
  }
  for (UINTN Try = 0; Try < 3; ++Try) {
    Drive (PIN_CS, FALSE);
    MicroSecondDelay (2);
    mLastRx[0] = SpiByte (Addr);
    mLastRx[1] = SpiByte (0xA0);
    mLastRx[2] = SpiByte ((UINT8)(Len >> 8));
    mLastRx[3] = SpiByte ((UINT8)(Len & 0xFF));
    SpiByte (0x00);
    SpiByte (0x00);
    SpiByte (0x00);
    for (UINTN I = 0; I < Len; ++I) {
      Out[I] = SpiByte (0x00);
    }
    UINT8 CrcLo = SpiByte (0x00);
    UINT8 CrcHi = SpiByte (0x00);
    MicroSecondDelay (2);
    Drive (PIN_CS, TRUE);
    MicroSecondDelay (10);
    ++mTransfers;
    if ((mLastRx[3] & 0xA0U) == 0) {
      UINT16 Expected = FtsCrc16 (Out, Len);
      UINT16 Actual = (UINT16)CrcLo | ((UINT16)CrcHi << 8);
      if (Expected == Actual) {
        return EFI_SUCCESS;
      }
    }
    MicroSecondDelay (1000);
  }
  return EFI_DEVICE_ERROR;
}

STATIC EFI_STATUS FtsWriteReg (UINT8 Addr, UINT8 Val) {
  for (UINTN Try = 0; Try < 3; ++Try) {
    Drive (PIN_CS, FALSE);
    MicroSecondDelay (2);
    SpiByte (Addr);
    SpiByte (0x00);
    SpiByte (0x00);
    UINT8 Ack = SpiByte (0x01);
    SpiByte (0x00);
    SpiByte (0x00);
    SpiByte (0x00);
    SpiByte (Val);
    MicroSecondDelay (2);
    Drive (PIN_CS, TRUE);
    MicroSecondDelay (10);
    if ((Ack & 0xA0U) == 0) {
      return EFI_SUCCESS;
    }
    MicroSecondDelay (1000);
  }
  return EFI_DEVICE_ERROR;
}

STATIC VOID PushState (UINT64 X, UINT64 Y, BOOLEAN Down) {
  UINTN Next = (mTail + 1) % QUEUE_LEN;
  if (Next == mHead) {
    mHead = (mHead + 1) % QUEUE_LEN;
  }
  EFI_ABSOLUTE_POINTER_STATE S;
  S.CurrentX = X;
  S.CurrentY = Y;
  S.CurrentZ = 0;
  S.ActiveButtons = Down ? EFI_ABSP_TouchActive : 0;
  mQueue[mTail] = S;
  mTail = Next;
  mLast = S;
  ++mEvents;
  if (mPointer.WaitForInput != NULL) {
    gBS->SignalEvent (mPointer.WaitForInput);
  }
}

STATIC VOID EFIAPI Poll (EFI_EVENT Event, VOID *Context) {
  (VOID)Event;
  (VOID)Context;
  if (!mRunning || mExited || mBusy) {
    return;
  }
  BOOLEAN Edge = (MmioRead32 (IRQ_STATUS (PIN_IRQ)) & BIT0) != 0;
  if (Edge) {
    MmioWrite32 (IRQ_STATUS (PIN_IRQ), 0);
  }
  mBusy = TRUE;
  UINT8 Buf[PKT_BYTES];
  EFI_STATUS ReadSt = FtsRead (0x01, Buf, sizeof (Buf));
  CopyMem (mLastBuf, Buf, 4);
  if (EFI_ERROR (ReadSt)) {
    ++mBad;
    mBusy = FALSE;
    return;
  }
  if (Buf[1] == 0xFF && Buf[2] == 0xFF && Buf[3] == 0xFF && Buf[4] == 0xFF) {
    ++mBad;
    mBusy = FALSE;
    return;
  }
  UINT8 Mode = Buf[1] >> 4;
  UINT8 Points = Buf[1] & 0x0F;
  if (Mode == 0 || Mode == 2) {
    ++mFrames;
    if (Points == 0) {
      if (mPressed) {
        mPressed = FALSE;
        PushState (mLast.CurrentX, mLast.CurrentY, FALSE);
      }
    } else {
      UINTN Off = (Mode == 2) ? 4U : 2U;
      UINT8 Ev = Buf[Off] >> 6;
      if (Ev == 1) {
        if (mPressed) {
          mPressed = FALSE;
          PushState (mLast.CurrentX, mLast.CurrentY, FALSE);
        }
      } else if (Ev == 0 || Ev == 2) {
        UINT32 Px, Py;
        if (Mode == 2) {
          UINT32 RawX = ((UINT32)(Buf[4] & 0x0F) << 12) | ((UINT32)Buf[5] << 4) | (Buf[8] >> 4);
          UINT32 RawY = ((UINT32)(Buf[6] & 0x0F) << 12) | ((UINT32)Buf[7] << 4) | (Buf[8] & 0x0F);
          Px = RawX >> 4;
          Py = RawY >> 4;
        } else {
          UINT32 RawX = ((UINT32)(Buf[2] & 0x0F) << 11) | ((UINT32)Buf[3] << 3) | ((Buf[6] >> 5) & 0x07U);
          UINT32 RawY = ((UINT32)(Buf[4] & 0x0F) << 11) | ((UINT32)Buf[5] << 3) | ((Buf[6] >> 3) & 0x07U);
          Px = RawX / 10U;
          Py = RawY / 10U;
        }
        if (Px > MAX_X) Px = MAX_X;
        if (Py > MAX_Y) Py = MAX_Y;
        UINT64 AbsX = ((UINT64)Px * 65535ULL) / MAX_X;
        UINT64 AbsY = ((UINT64)Py * 65535ULL) / MAX_Y;
        mPressed = TRUE;
        PushState (AbsX, AbsY, TRUE);
      }
    }
  }
  mBusy = FALSE;
}

STATIC VOID EFIAPI WaitForInput (EFI_EVENT Event, VOID *Context) {
  (VOID)Context;
  if (!mExited && mHead != mTail) {
    gBS->SignalEvent (Event);
  }
}

STATIC EFI_STATUS EFIAPI Reset (EFI_ABSOLUTE_POINTER_PROTOCOL *This, BOOLEAN Extended) {
  (VOID)Extended;
  if (This != &mPointer) {
    return EFI_INVALID_PARAMETER;
  }
  if (mExited || !mRunning) {
    return EFI_NOT_READY;
  }
  EFI_TPL Old = gBS->RaiseTPL (TPL_NOTIFY);
  mHead = mTail = 0;
  mPressed = FALSE;
  ZeroMem (&mLast, sizeof (mLast));
  gBS->RestoreTPL (Old);
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI GetState (EFI_ABSOLUTE_POINTER_PROTOCOL *This, EFI_ABSOLUTE_POINTER_STATE *State) {
  if (This != &mPointer || State == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  if (mExited || (!mRunning && mHead == mTail)) {
    return EFI_NOT_READY;
  }
  EFI_TPL Old = gBS->RaiseTPL (TPL_NOTIFY);
  EFI_STATUS Status = EFI_NOT_READY;
  if (mHead != mTail) {
    *State = mQueue[mHead];
    mHead = (mHead + 1) % QUEUE_LEN;
    Status = EFI_SUCCESS;
  }
  gBS->RestoreTPL (Old);
  return Status;
}

STATIC VOID EFIAPI Fence (EFI_EVENT Event, VOID *Context) {
  (VOID)Event;
  (VOID)Context;
  mExited = TRUE;
  if (mPinsSaved || mPowerSaved || mIrqOwned) {
    mRetained = TRUE;
  }
}

STATIC EFI_STATUS EnableIovdd (VOID) {
  EFI_GUID RpmhGuid = {0xe1d0b573, 0xf29f, 0x495d, {0x93, 0x52, 0xca, 0x0d, 0xd4, 0x2a, 0x0a, 0x74}};
  EFI_GUID CmdDbGuid = {0x1c34f691, 0xd33d, 0x4e14, {0xb8, 0xf8, 0x32, 0xc6, 0xc0, 0x29, 0xe9, 0x5b}};
  MIRO_CMD_DB *Db = NULL;
  mRailStep = 1;
  EFI_STATUS S = gBS->LocateProtocol (&RpmhGuid, NULL, (VOID **)&mRpmh);
  if (S != EFI_SUCCESS || mRpmh == NULL) {
    return S != EFI_SUCCESS ? S : EFI_UNSUPPORTED;
  }
  mRailStep = 2;
  S = gBS->LocateProtocol (&CmdDbGuid, NULL, (VOID **)&Db);
  if (S != EFI_SUCCESS || Db == NULL) {
    return S != EFI_SUCCESS ? S : EFI_UNSUPPORTED;
  }
  mRailStep = 3;
  if (mRpmh->Revision != 0x10001 || Db->Revision != 0x10000) {
    return EFI_UNSUPPORTED;
  }
  mRailStep = 4;
  BOOLEAN Valid = FALSE;
  S = mRpmh->ValidDrv (2, &Valid);
  if (S != EFI_SUCCESS || !Valid) {
    return EFI_UNSUPPORTED;
  }
  mRailStep = 5;
  UINT32 Base = 0;
  S = Db->Address (Db, "ldob12", &Base);
  mRailBase = Base;
  if (S != EFI_SUCCESS) {
    return S;
  }
  mRailStep = 6;
  if ((Base >> 16) != 4 || (Base & 255)) {
    return EFI_UNSUPPORTED;
  }
  mRailStep = 7;
  for (UINTN J = 0; J < 3; ++J) {
    UINT32 Address = Base + (UINT32)J * 4, Old = 0;
    MIRO_RPMH_RESOURCE *Res = NULL;
    S = mRpmh->Find (Address, (VOID **)&Res);
    if (S != EFI_SUCCESS) {
      return S;
    }
    if (Res != NULL) {
      MIRO_RPMH_DRV *Vote = Res->Votes;
      for (UINTN N = 0; Vote != NULL && N < 16; Vote = Vote->Next, ++N) {
        if (Vote->Drv == 2) {
          break;
        }
      }
      if (Vote != NULL && Vote->Local[0].Valid) {
        Old = Vote->Local[0].Data;
      }
    }
    UINT32 Want = J == 0 ? 1800 : J == 1 ? 1 : 7;
    mLdo12[J].Address = Address;
    mLdo12[J].Before = Old;
    mLdo12[J].After = MAX (Old, Want);
  }
  S = mRpmh->Create (2, "SunUEFI Miro Touch", &mRpmhHandle);
  if (S != EFI_SUCCESS || mRpmhHandle == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  for (UINTN J = 0; J < 3; ++J) {
    UINT32 Req = 0;
    S = mRpmh->Issue (mRpmhHandle, 0, TRUE, mLdo12[J].Address, mLdo12[J].After, &Req);
    if (S == EFI_SUCCESS) {
      S = mRpmh->Churn (mRpmhHandle, Req);
    }
    if (S != EFI_SUCCESS) {
      return S;
    }
    mLdo12[J].Changed = TRUE;
  }
  gBS->Stall (2000);
  return EFI_SUCCESS;
}

STATIC VOID DisableIovdd (VOID) {
  if (mRpmh == NULL || mRpmhHandle == NULL) {
    return;
  }
  for (UINTN J = 3; J > 0; --J) {
    if (mLdo12[J - 1].Changed) {
      UINT32 Req = 0;
      if (mRpmh->Issue (mRpmhHandle, 0, TRUE, mLdo12[J - 1].Address, mLdo12[J - 1].Before, &Req) == EFI_SUCCESS) {
        (VOID)mRpmh->Churn (mRpmhHandle, Req);
      }
      mLdo12[J - 1].Changed = FALSE;
    }
  }
  (VOID)mRpmh->Delete (mRpmhHandle);
  mRpmhHandle = NULL;
}

STATIC EFI_STATUS AcquireHardware (VOID) {
  mRailStatus = EnableIovdd ();
  mAvddCtl = MmioRead32 (PIN_CTL (PIN_AVDD));
  mAvddIo = MmioRead32 (PIN_IO (PIN_AVDD));
  mRstCtl = MmioRead32 (PIN_CTL (PIN_RESET));
  mRstIo = MmioRead32 (PIN_IO (PIN_RESET));
  mPowerSaved = TRUE;

  MmioWrite32 (PIN_CTL (PIN_AVDD), GpioMuxConfig (mAvddCtl, TRUE, 0));
  Drive (PIN_AVDD, TRUE);
  MmioWrite32 (PIN_CTL (PIN_RESET), GpioMuxConfig (mRstCtl, TRUE, 0));
  Drive (PIN_RESET, FALSE);

  for (UINTN I = 0; I < 4; ++I) {
    mSavedCtl[I] = MmioRead32 (PIN_CTL (mSpiPins[I]));
    mSavedIo[I] = MmioRead32 (PIN_IO (mSpiPins[I]));
  }
  mPinsSaved = TRUE;

  MmioWrite32 (PIN_CTL (PIN_MISO), GpioMuxConfig (mSavedCtl[0], FALSE, 0));
  MmioWrite32 (PIN_CTL (PIN_MOSI), GpioMuxConfig (mSavedCtl[1], TRUE, 0));
  MmioWrite32 (PIN_CTL (PIN_CLK), GpioMuxConfig (mSavedCtl[2], TRUE, 0));
  MmioWrite32 (PIN_CTL (PIN_CS), GpioMuxConfig (mSavedCtl[3], TRUE, 0));
  Drive (PIN_CS, TRUE);
  Drive (PIN_CLK, FALSE);
  Drive (PIN_MOSI, FALSE);

  mIrqCtl = MmioRead32 (PIN_CTL (PIN_IRQ));
  mIrqCfg = MmioRead32 (IRQ_CFG (PIN_IRQ));
  mIrqOwned = TRUE;
  MmioWrite32 (IRQ_CFG (PIN_IRQ), mIrqCfg & ~(BIT0 | BIT7));
  UINT32 IrqCtl = (mIrqCtl & ~(0x3CU | BIT9)) | 3U; // pull-up
  if (IrqCtl & BIT11) {
    IrqCtl |= BIT12;
  }
  MmioWrite32 (PIN_CTL (PIN_IRQ), IrqCtl);
  UINT32 IrqCfg = (mIrqCfg & ~0x8FU) | BIT4 | BIT3; // falling edge raw capture (POL=0, DECT=2)
  MmioWrite32 (IRQ_CFG (PIN_IRQ), IrqCfg);
  MmioWrite32 (IRQ_STATUS (PIN_IRQ), 0);

  MicroSecondDelay (10000);
  Drive (PIN_RESET, TRUE);
  MicroSecondDelay (200000);
  return EFI_SUCCESS;
}

STATIC EFI_STATUS ReleaseHardware (VOID) {
  if (mIrqOwned) {
    MmioAnd32 (IRQ_CFG (PIN_IRQ), ~(BIT0 | BIT7));
    MmioWrite32 (IRQ_STATUS (PIN_IRQ), 0);
    MmioWrite32 (PIN_CTL (PIN_IRQ), mIrqCtl);
    MmioWrite32 (IRQ_CFG (PIN_IRQ), mIrqCfg);
    mIrqOwned = FALSE;
  }
  if (mPinsSaved) {
    Drive (PIN_CS, TRUE);
    Drive (PIN_CLK, FALSE);
    for (UINTN I = 0; I < 4; ++I) {
      MmioAnd32 (PIN_CTL (mSpiPins[I]), ~BIT9);
      MmioWrite32 (PIN_IO (mSpiPins[I]), mSavedIo[I] & BIT1);
      MmioWrite32 (PIN_CTL (mSpiPins[I]), mSavedCtl[I]);
    }
    mPinsSaved = FALSE;
  }
  if (mPowerSaved) {
    MmioWrite32 (PIN_IO (PIN_RESET), mRstIo & BIT1);
    MmioWrite32 (PIN_CTL (PIN_RESET), mRstCtl);
    MmioWrite32 (PIN_IO (PIN_AVDD), mAvddIo & BIT1);
    MmioWrite32 (PIN_CTL (PIN_AVDD), mAvddCtl);
    mPowerSaved = FALSE;
  }
  DisableIovdd ();
  return EFI_SUCCESS;
}

BOOLEAN MiroTouchRetained (VOID) {
  return mRetained;
}

VOID MiroTouchReplay (VOID) {
  DEBUG ((DEBUG_WARN, "SUNUEFI_MIRO_TOUCH status=%r id=%02x%02x running=%u retained=%u frames=%u events=%u bad=%u transfers=%u\n",
    mStatus, mChipIdH, mChipIdL, mRunning, mRetained, mFrames, mEvents, mBad, mTransfers));
}

VOID MiroTouchDiag (UINT32 *Id, EFI_STATUS *Status, UINT32 *Frames, UINT32 *Events, UINT32 *Bad, UINT32 *IrqState) {
  *Id = ((UINT32)mChipIdH << 8) | mChipIdL;
  *Status = mStatus != EFI_SUCCESS && mRailStatus != EFI_SUCCESS ? mRailStatus : mStatus;
  *Frames = mFrames;
  *Events = mEvents;
  *Bad = mBad;
  *IrqState = ((UINT32)mLastBuf[0] << 24) | ((UINT32)mLastBuf[1] << 16) | ((UINT32)mLastRx[3] << 8) | (MmioRead32 (PIN_IO (PIN_IRQ)) & 1U);
}

EFI_STATUS MiroTouchStop (VOID *Context, PIANO_PRODUCT_INPUT_RETIRE_REPORT *Report) {
  (VOID)Context;
  if (Report == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  ZeroMem (Report, sizeof (*Report));
  Report->Revision = 1;
  Report->Started = mRunning;
  if (mExited || mRetained) {
    Report->Retained = TRUE;
    return Report->Status = EFI_ACCESS_DENIED;
  }
  mRunning = FALSE;
  if (mTimer != NULL) {
    gBS->SetTimer (mTimer, TimerCancel, 0);
    gBS->CloseEvent (mTimer);
    mTimer = NULL;
  }
  if (mHandle != NULL) {
    gBS->DisconnectController (mHandle, NULL, NULL);
    gBS->UninstallMultipleProtocolInterfaces (mHandle,
      &gEfiAbsolutePointerProtocolGuid, &mPointer,
      &gEfiConsoleInDeviceGuid, NULL,
      &gEfiDevicePathProtocolGuid, &mPath, NULL);
    mHandle = NULL;
  }
  if (mPointer.WaitForInput != NULL) {
    gBS->CloseEvent (mPointer.WaitForInput);
    mPointer.WaitForInput = NULL;
  }
  if (mExit != NULL) {
    gBS->CloseEvent (mExit);
    mExit = NULL;
  }
  ReleaseHardware ();
  Report->Returned = TRUE;
  Report->Protocols = EFI_SUCCESS;
  Report->Timer = EFI_SUCCESS;
  Report->Clean = TRUE;
  Report->Retained = FALSE;
  return Report->Status = EFI_SUCCESS;
}

EFI_STATUS MiroTouchStart (CONST VOID *Fdt) {
  if (Fdt == NULL || FdtCheckHeader (Fdt)) {
    return EFI_INVALID_PARAMETER;
  }
  INT32 Node = FdtPathOffset (Fdt, TOUCH_NODE), Len = 0;
  CONST CHAR8 *Compat = Node < 0 ? NULL : FdtGetProp (Fdt, Node, "compatible", &Len);
  if (Compat == NULL || Len < 15 || AsciiStrnCmp (Compat, "focal,touch-spi", 15) != 0) {
    return EFI_UNSUPPORTED;
  }
  if (mRunning || mPinsSaved || mPowerSaved) {
    return EFI_ALREADY_STARTED;
  }
  AcquireHardware ();
  for (UINTN Try = 0; Try < 5; ++Try) {
    FtsRead (0xA3, &mChipIdH, 1);
    FtsRead (0x9F, &mChipIdL, 1);
    if (mChipIdH == 0x36 && mChipIdL == 0x83) {
      break;
    }
    MicroSecondDelay (50000);
  }
  DEBUG ((DEBUG_WARN, "SUNUEFI_MIRO_TOUCH_ID id=%02x%02x\n", mChipIdH, mChipIdL));
  if (mChipIdH != 0x36 || mChipIdL != 0x83) {
    ReleaseHardware ();
    mStatus = EFI_NOT_FOUND;
    return EFI_NOT_FOUND;
  }
  UINT8 Reg9E = 0, RegA5 = 0;
  if (FtsRead (0x9E, &Reg9E, 1) == EFI_SUCCESS && Reg9E != 0) {
    FtsWriteReg (0x9E, 0x00);
    MicroSecondDelay (20000);
  }
  if (FtsRead (0xA5, &RegA5, 1) == EFI_SUCCESS && RegA5 != 0) {
    FtsWriteReg (0xA5, 0x00);
    MicroSecondDelay (20000);
  }
  mPointer.Reset = Reset;
  mPointer.GetState = GetState;
  mPointer.Mode = &mMode;
  EFI_STATUS Status = gBS->CreateEvent (EVT_NOTIFY_WAIT, TPL_NOTIFY, WaitForInput, NULL, &mPointer.WaitForInput);
  if (Status == EFI_SUCCESS) {
    Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_NOTIFY, Fence, NULL, &gEfiEventExitBootServicesGuid, &mExit);
  }
  if (Status == EFI_SUCCESS) {
    Status = gBS->InstallMultipleProtocolInterfaces (&mHandle,
      &gEfiAbsolutePointerProtocolGuid, &mPointer,
      &gEfiConsoleInDeviceGuid, NULL,
      &gEfiDevicePathProtocolGuid, &mPath, NULL);
  }
  if (Status == EFI_SUCCESS) {
    Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Poll, NULL, &mTimer);
  }
  if (Status == EFI_SUCCESS) {
    mRunning = TRUE;
    Status = gBS->SetTimer (mTimer, TimerPeriodic, 10ULL * 10000ULL);
  }
  if (Status == EFI_SUCCESS) {
    (VOID)gBS->ConnectController (mHandle, NULL, NULL, TRUE);
  } else {
    PIANO_PRODUCT_INPUT_RETIRE_REPORT Dummy;
    MiroTouchStop (NULL, &Dummy);
  }
  mStatus = Status;
  MiroTouchReplay ();
  return Status;
}
