// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2015, Lourens Rozema <me@LourensRozema.nl>. All rights reserved.

/*- Includes ----------------------------------------------------------------*/
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#include "hidapi.h"
#include "edbg.h"
#include "dbg.h"

/*- Variables ---------------------------------------------------------------*/
static hid_device *handle = NULL;
static uint8_t hid_buffer[DBG_MAX_EP_SIZE + 1];
static int report_size = 64; // TODO: read actual report size

/*- Implementations ---------------------------------------------------------*/

//-----------------------------------------------------------------------------
char *wcstombsdup(const wchar_t * const src)
{
  const int len = wcslen(src);
  char * const dst = malloc(len + 1);

  if (dst)
  {
    wcstombs(dst, src, len);
    dst[len] = '\0';
  }

  return dst;
}

//-----------------------------------------------------------------------------
int dbg_enumerate(debugger_t *debuggers, int size)
{
  struct hid_device_info *devs, *cur_dev;
  int rsize = 0;

  if (hid_init())
    return 0;

  devs = hid_enumerate(0, 0);
  cur_dev = devs;

  for (cur_dev = devs; cur_dev && rsize < size; cur_dev = cur_dev->next)
  {
    debuggers[rsize].path = strdup(cur_dev->path);
    debuggers[rsize].serial = cur_dev->serial_number ? wcstombsdup(cur_dev->serial_number) : "<unknown>";
    debuggers[rsize].wserial = cur_dev->serial_number ? wcsdup(cur_dev->serial_number) : NULL;
    debuggers[rsize].manufacturer = cur_dev->manufacturer_string ? wcstombsdup(cur_dev->manufacturer_string) : "<unknown>";
    debuggers[rsize].product = cur_dev->product_string ? wcstombsdup(cur_dev->product_string) : "<unknown>";
    debuggers[rsize].vid = cur_dev->vendor_id;
    debuggers[rsize].pid = cur_dev->product_id;

    if (strstr(debuggers[rsize].product, "CMSIS-DAP"))
      rsize++;
  }

  hid_free_enumeration(devs);

  return rsize;
}

//-----------------------------------------------------------------------------
void dbg_open(debugger_t *debugger)
{
  handle = hid_open(debugger->vid, debugger->pid, debugger->wserial);

  if (!handle)
    perror_exit("unable to open device");
}

//-----------------------------------------------------------------------------
void dbg_close(void)
{
  if (handle)
    hid_close(handle);
}

//-----------------------------------------------------------------------------
int dbg_get_report_size(void)
{
  return report_size;
}

//-----------------------------------------------------------------------------
int dbg_dap_cmd(uint8_t *data, int resp_size, int req_size)
{
  uint8_t cmd = data[0];
  int res;

  memset(hid_buffer, 0xff, report_size + 1);

  hid_buffer[0] = 0x00; // Report ID
  memcpy(&hid_buffer[1], data, req_size);

  res = hid_write(handle, hid_buffer, report_size + 1);
  if (res < 0)
  {
    message("Error: %ls\n", hid_error(handle));
    perror_exit("debugger write()");
  }

  res = hid_read(handle, hid_buffer, report_size + 1);
  if (res < 0)
    perror_exit("debugger read()");

  check(res, "empty response received");

  if (hid_buffer[0] != cmd)
    error_exit("invalid response received: request = 0x%02x, response = 0x%02x", cmd, hid_buffer[0]);

  res--;
  memcpy(data, &hid_buffer[1], (resp_size < res) ? resp_size : res);

  return res;
}
