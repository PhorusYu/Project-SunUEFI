// SPDX-License-Identifier: BSD-2-Clause-Patent
#pragma once
#include <Uefi.h>
#include "PianoProductOwners.h"

EFI_STATUS MiroTouchStart (CONST VOID *Fdt);
EFI_STATUS MiroTouchStop (VOID *Context, PIANO_PRODUCT_INPUT_RETIRE_REPORT *Report);
VOID MiroTouchReplay (VOID);
BOOLEAN MiroTouchRetained (VOID);
VOID MiroTouchDiag (UINT32 *Id, EFI_STATUS *Status, UINT32 *Frames, UINT32 *Events, UINT32 *Bad, UINT32 *IrqState);
