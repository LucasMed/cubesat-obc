/* test_fw_upload.c — Unit tests for firmware upload state machine
 *
 * Tests the upload state machine on host build.  W25Q64 hardware paths
 * are inside #ifdef PICO_BUILD so they are NOT exercised here — only
 * state flow, error handling, and parameter validation are tested.
 *
 * T-FWU-01  test_fwu_initial_state    – Initial state is IDLE
 * T-FWU-02  test_fwu_start_basic      – Start → RECEIVING
 * T-FWU-03  test_fwu_chunk_sequence    – Write chunks in correct order
 * T-FWU-04  test_fwu_auto_complete     – Final chunk triggers COMPLETE
 * T-FWU-05  test_fwu_start_busy        – Start rejected when already in progress
 * T-FWU-06  test_fwu_chunk_wrong_seq   – Wrong seq rejected
 * T-FWU-07  test_fwu_chunk_wrong_state – Chunk rejected from IDLE
 * T-FWU-08  test_fwu_chunk_too_large   – Oversized chunk rejected
 * T-FWU-09  test_fwu_verify_wrong_state– Verify rejected from RECEIVING
 * T-FWU-10  test_fwu_verify_ok         – Verify passes from COMPLETE
 * T-FWU-11  test_fwu_commit_wrong_state– Commit rejected from RECEIVING
 * T-FWU-12  test_fwu_commit_ok         – Commit passes from VERIFIED
 * T-FWU-13  test_fwu_abort             – Abort returns to IDLE
 * T-FWU-14  test_fwu_is_busy           – is_busy reflects RECEIVING state
 * T-FWU-15  test_fwu_get_status        – get_status returns correct info
 * T-FWU-16  test_fwu_invalid_params    – Invalid total_size rejected
 * T-FWU-17  test_fwu_full_cycle        – Full START→CHUNKs→VERIFY→COMMIT→IDLE
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* fw_upload public API — compiled as separate translation unit (fw_upload.c linked). */
#include "fw_upload.h"
#include "boot_info.h"

static int g_failures = 0;

#define CHECK(cond, msg)                                            \
  do {                                                              \
    if (!(cond)) {                                                  \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);       \
      g_failures++;                                                 \
    }                                                               \
  } while (0)

/* Helper: check that upload status matches expected values */
static void check_status(uint32_t total, uint32_t crc, uint8_t slot,
                         uint32_t received, uint32_t chunks,
                         fw_upload_state_t state)
{
  fw_upload_status_t st;
  fw_upload_get_status(&st);
  CHECK(st.state        == state,    "State matches");
  CHECK(st.total_size   == total,    "Total size matches");
  CHECK(st.expected_crc == crc,      "Expected CRC matches");
  CHECK(st.received     == received, "Received bytes match");
  CHECK(st.chunk_count  == chunks,   "Chunk count matches");
  CHECK(st.target_slot  == slot,     "Target slot matches");
}

/* ================================================================
 * T-FWU-01: Initial state
 * ================================================================ */
void test_fwu_initial_state(void)
{
  printf("[%s]\n", __func__);
  check_status(0, 0, 0, 0, 0, FW_STATE_IDLE);
  CHECK(!fw_upload_is_busy(), "Not busy in IDLE");
}

/* ================================================================
 * T-FWU-02: Start → RECEIVING
 * ================================================================ */
void test_fwu_start_basic(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();  /* Ensure clean state */

  int ret = fw_upload_start(1024, 0x12345678, BOOT_SLOT_A);
  CHECK(ret == 0,         "fw_upload_start returns 0");
  CHECK(fw_upload_is_busy(), "Busy after start");
  check_status(1024, 0x12345678, BOOT_SLOT_A, 0, 0, FW_STATE_RECEIVING);
}

/* ================================================================
 * T-FWU-03: Chunk sequence (correct order)
 * ================================================================ */
void test_fwu_chunk_sequence(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(768, 0, BOOT_SLOT_B);

  uint8_t chunk[256];
  memset(chunk, 0xAB, sizeof(chunk));

  int ret;

  /* First chunk (seq 0) */
  ret = fw_upload_write_chunk(0, chunk, 256);
  CHECK(ret == 0, "Chunk 0 accepted");
  check_status(768, 0, BOOT_SLOT_B, 256, 1, FW_STATE_RECEIVING);

  /* Second chunk (seq 1) */
  ret = fw_upload_write_chunk(1, chunk, 256);
  CHECK(ret == 0, "Chunk 1 accepted");
  check_status(768, 0, BOOT_SLOT_B, 512, 2, FW_STATE_RECEIVING);

  /* Third chunk (seq 2) — smaller (256 B last chunk for 768 total) */
  ret = fw_upload_write_chunk(2, chunk, 256);
  CHECK(ret == 0, "Chunk 2 accepted");
  /* 768 total reached → should auto-transition to COMPLETE */
  check_status(768, 0, BOOT_SLOT_B, 768, 3, FW_STATE_COMPLETE);
}

/* ================================================================
 * T-FWU-04: Auto-complete when received >= total_size
 * ================================================================ */
void test_fwu_auto_complete(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(256, 0, BOOT_SLOT_A);

  uint8_t chunk[256];
  memset(chunk, 0x42, sizeof(chunk));

  /* Single chunk fills the entire 256-byte image */
  int ret = fw_upload_write_chunk(0, chunk, 128);
  CHECK(ret == 0, "Chunk 0 accepted (partial)");
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_RECEIVING, "Still RECEIVING after partial"); }

  ret = fw_upload_write_chunk(1, chunk, 128);
  CHECK(ret == 0, "Chunk 1 accepted");
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_COMPLETE, "COMPLETE after full image"); }
}

/* ================================================================
 * T-FWU-05: Start rejected when already in progress
 * ================================================================ */
void test_fwu_start_busy(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(512, 0, BOOT_SLOT_A);
  int ret = fw_upload_start(256, 0, BOOT_SLOT_B);
  CHECK(ret == -1, "Start returns -1 when busy");
}

/* ================================================================
 * T-FWU-06: Wrong chunk sequence
 * ================================================================ */
void test_fwu_chunk_wrong_seq(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(1024, 0, BOOT_SLOT_A);

  uint8_t chunk[256];
  memset(chunk, 0, sizeof(chunk));

  /* Send seq 5 first (should be seq 0) */
  int ret = fw_upload_write_chunk(5, chunk, 256);
  CHECK(ret == -2, "Wrong seq returns -2");
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_RECEIVING, "Still RECEIVING after bad seq"); }
  check_status(1024, 0, BOOT_SLOT_A, 0, 0, FW_STATE_RECEIVING);
}

/* ================================================================
 * T-FWU-07: Chunk rejected from wrong state (IDLE)
 * ================================================================ */
void test_fwu_chunk_wrong_state(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  /* No start called — state should be IDLE */

  uint8_t chunk[256];
  memset(chunk, 0, sizeof(chunk));
  int ret = fw_upload_write_chunk(0, chunk, 256);
  CHECK(ret == -1, "Chunk from IDLE returns -1");
}

/* ================================================================
 * T-FWU-08: Oversized chunk rejected
 * ================================================================ */
void test_fwu_chunk_too_large(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(512, 0, BOOT_SLOT_A);

  uint8_t oversized[300];
  memset(oversized, 0, sizeof(oversized));
  int ret = fw_upload_write_chunk(0, oversized, 300);
  CHECK(ret == -2, "Oversized chunk returns -2");
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_RECEIVING, "Still RECEIVING after bad chunk"); }
}

/* ================================================================
 * T-FWU-09: Verify rejected from RECEIVING
 * ================================================================ */
void test_fwu_verify_wrong_state(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(256, 0, BOOT_SLOT_A);
  /* Don't send any chunks — still in RECEIVING */

  int ret = fw_upload_verify();
  CHECK(ret == -2, "Verify from RECEIVING returns -2");
}

/* ================================================================
 * T-FWU-10: Verify passes from COMPLETE
 *
 * On host build, verify auto-passes (PICO_BUILD not defined).
 * ================================================================ */
void test_fwu_verify_ok(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(256, 0xDEADBEEF, BOOT_SLOT_A);

  uint8_t chunk[256];
  memset(chunk, 0xFF, sizeof(chunk));
  fw_upload_write_chunk(0, chunk, 256);
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_COMPLETE, "COMPLETE after full image"); }

  int ret = fw_upload_verify();
  CHECK(ret == 0, "Verify returns 0 on host (auto-pass)");
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_VERIFIED, "VERIFIED after verify"); }
}

/* ================================================================
 * T-FWU-11: Commit rejected from RECEIVING
 * ================================================================ */
void test_fwu_commit_wrong_state(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(256, 0, BOOT_SLOT_A);

  int ret = fw_upload_commit();
  CHECK(ret == -2, "Commit from RECEIVING returns -2");
}

/* ================================================================
 * T-FWU-12: Commit passes from VERIFIED
 *
 * On host build, commit auto-passes (PICO_BUILD not defined).
 * ================================================================ */
void test_fwu_commit_ok(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(256, 0, BOOT_SLOT_A);

  uint8_t chunk[256];
  memset(chunk, 0xFF, sizeof(chunk));
  fw_upload_write_chunk(0, chunk, 256);
  fw_upload_verify();

  int ret = fw_upload_commit();
  CHECK(ret == 0, "Commit returns 0 on host (auto-pass)");
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_IDLE, "IDLE after commit"); }
}

/* ================================================================
 * T-FWU-13: Abort
 * ================================================================ */
void test_fwu_abort(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(1024, 0, BOOT_SLOT_B);

  uint8_t chunk[256];
  memset(chunk, 0, sizeof(chunk));
  fw_upload_write_chunk(0, chunk, 256);
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_RECEIVING, "RECEIVING before abort"); }

  fw_upload_abort();
  check_status(0, 0, 0, 0, 0, FW_STATE_IDLE);
  CHECK(!fw_upload_is_busy(), "Not busy after abort");
}

/* ================================================================
 * T-FWU-14: is_busy
 * ================================================================ */
void test_fwu_is_busy(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  CHECK(!fw_upload_is_busy(), "Not busy in IDLE");

  fw_upload_start(256, 0, BOOT_SLOT_A);
  CHECK(fw_upload_is_busy(), "Busy in RECEIVING");

  fw_upload_abort();
  CHECK(!fw_upload_is_busy(), "Not busy after abort");
}

/* ================================================================
 * T-FWU-15: get_status
 * ================================================================ */
void test_fwu_get_status(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();
  fw_upload_start(2048, 0xAABBCCDD, BOOT_SLOT_A);

  /* Send two chunks */
  uint8_t chunk[256];
  memset(chunk, 0xAA, sizeof(chunk));
  fw_upload_write_chunk(0, chunk, 256);
  fw_upload_write_chunk(1, chunk, 256);

  fw_upload_status_t st;
  fw_upload_get_status(&st);

  CHECK(st.state        == FW_STATE_RECEIVING, "State RECEIVING");
  CHECK(st.total_size   == 2048,              "Total size 2048");
  CHECK(st.expected_crc == 0xAABBCCDD,        "Expected CRC");
  CHECK(st.received     == 512,               "Received 512 bytes");
  CHECK(st.chunk_count  == 2,                 "2 chunks");
  CHECK(st.target_slot  == BOOT_SLOT_A,       "Slot A");

  /* Null pointer safety */
  fw_upload_get_status(NULL);
  printf("  Null pointer test: PASS\n");
}

/* ================================================================
 * T-FWU-16: Invalid parameters
 * ================================================================ */
void test_fwu_invalid_params(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();

  /* Zero total_size */
  int ret = fw_upload_start(0, 0, BOOT_SLOT_A);
  CHECK(ret == -2, "Zero total_size returns -2");

  /* Exceeding MAX_IMAGE_SIZE (1 MB = 0x100000) */
  ret = fw_upload_start(0x100001, 0, BOOT_SLOT_A);
  CHECK(ret == -2, "Oversized total_size returns -2");

  /* After invalid start, state should still be IDLE */
  check_status(0, 0, 0, 0, 0, FW_STATE_IDLE);
}

/* ================================================================
 * T-FWU-17: Full cycle
 * ================================================================ */
void test_fwu_full_cycle(void)
{
  printf("[%s]\n", __func__);
  fw_upload_abort();

  /* Full START → CHUNKs → VERIFY → COMMIT cycle */
  int ret;

  ret = fw_upload_start(512, 0x12345678, BOOT_SLOT_B);
  CHECK(ret == 0,            "Start");
  CHECK(fw_upload_is_busy(), "Busy after start");

  uint8_t chunk[256];
  memset(chunk, 0x55, sizeof(chunk));
  ret = fw_upload_write_chunk(0, chunk, 256);
  CHECK(ret == 0, "Chunk 0");
  ret = fw_upload_write_chunk(1, chunk, 256);
  CHECK(ret == 0, "Chunk 1");
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_COMPLETE, "COMPLETE"); }

  ret = fw_upload_verify();
  CHECK(ret == 0, "Verify");
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_VERIFIED, "VERIFIED"); }

  ret = fw_upload_commit();
  CHECK(ret == 0, "Commit");
  { fw_upload_status_t st; fw_upload_get_status(&st);
    CHECK(st.state == FW_STATE_IDLE, "IDLE after commit"); }
  CHECK(!fw_upload_is_busy(), "Not busy after commit");
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void)
{
  printf("=== Firmware Upload State Machine Tests ===\n\n");

  test_fwu_initial_state();        printf("\n");
  test_fwu_start_basic();          printf("\n");
  test_fwu_chunk_sequence();       printf("\n");
  test_fwu_auto_complete();        printf("\n");
  test_fwu_start_busy();           printf("\n");
  test_fwu_chunk_wrong_seq();      printf("\n");
  test_fwu_chunk_wrong_state();    printf("\n");
  test_fwu_chunk_too_large();      printf("\n");
  test_fwu_verify_wrong_state();   printf("\n");
  test_fwu_verify_ok();            printf("\n");
  test_fwu_commit_wrong_state();   printf("\n");
  test_fwu_commit_ok();            printf("\n");
  test_fwu_abort();                printf("\n");
  test_fwu_is_busy();              printf("\n");
  test_fwu_get_status();           printf("\n");
  test_fwu_invalid_params();       printf("\n");
  test_fwu_full_cycle();           printf("\n");

  printf("=== Results: %d failures ===\n", g_failures);
  return g_failures > 0 ? 1 : 0;
}
