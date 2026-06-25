/**
 * @file fw_upload.h
 * @brief Firmware upload state machine for CSP OTA updates.
 *
 * Manages the staging of firmware images to the W25Q64 external flash
 * before committing to the inactive internal flash slot.
 *
 * Upload flow:
 *   UPLOAD_START → [UPLOAD_CHUNK × N] → UPLOAD_VERIFY → UPLOAD_COMMIT
 *   Any step → UPLOAD_ABORT → IDLE
 *
 * Spec ref: firmware-update/spec.md, OBC-DES-001 §4.6
 */

#ifndef FW_UPLOAD_H
#define FW_UPLOAD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

/** Maximum number of firmware chunks in a single upload. */
#define FW_UPLOAD_MAX_CHUNKS 4096u

/** Payload bytes per firmware chunk. */
#define FW_UPLOAD_CHUNK_SIZE 256u

/** Maximum firmware image size (256 B × 4096 = 1 MB). */
#define FW_UPLOAD_MAX_IMAGE_SIZE 0x100000u

/** W25Q64 staging offset (1 MB for OTA staging buffer). */
#define FW_STAGING_OFFSET 0x200000u
#define FW_STAGING_SIZE 0x100000u

  /* ------------------------------------------------------------------ */
  /* Upload state                                                        */
  /* ------------------------------------------------------------------ */

  typedef enum
  {
    FW_STATE_IDLE = 0,       /**< No upload in progress               */
    FW_STATE_RECEIVING = 1,  /**< Accepting chunks                    */
    FW_STATE_COMPLETE = 2,   /**< All chunks received, ready to verify*/
    FW_STATE_VERIFIED = 3,   /**< CRC32 of staging matches expected   */
    FW_STATE_COMMITTING = 4, /**< Programming inactive slot (blocking)*/
    FW_STATE_FAILED = 5      /**< Upload aborted or error             */
  } fw_upload_state_t;

  /* ------------------------------------------------------------------ */
  /* Upload status structure                                             */
  /* ------------------------------------------------------------------ */

  typedef struct __attribute__((packed))
  {
    fw_upload_state_t state; /**< Current upload state              */
    uint32_t total_size;     /**< Expected total size in bytes     */
    uint32_t expected_crc;   /**< Expected CRC32 of the image      */
    uint32_t received;       /**< Bytes received so far            */
    uint32_t chunk_count;    /**< Number of chunks received        */
    uint8_t target_slot;     /**< BOOT_SLOT_A / BOOT_SLOT_B       */
  } fw_upload_status_t;

  /* ------------------------------------------------------------------ */
  /* Public API                                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Start a firmware upload session.
   *
   * Erases the W25Q64 staging region and initialises the upload state
   * machine.  Must be called before sending any chunks.
   *
   * @param total_size  Expected firmware image size in bytes.
   * @param expected_crc  Expected CRC32 of the complete image.
   * @param target_slot  BOOT_SLOT_A or BOOT_SLOT_B (inactive slot).
   * @return 0 on success, -1 if already in progress, -2 if invalid args.
   */
  int fw_upload_start(uint32_t total_size, uint32_t expected_crc, uint8_t target_slot);

  /**
   * @brief Write one chunk of firmware data to W25Q64 staging.
   *
   * Chunks are written sequentially at FW_STAGING_OFFSET.
   *
   * @param seq  Chunk sequence number (0-based).
   * @param data  Chunk payload (FW_UPLOAD_CHUNK_SIZE bytes, last may be shorter).
   * @param len   Length of this chunk (<= FW_UPLOAD_CHUNK_SIZE).
   * @return 0 on success, -1 if state != RECEIVING, -2 if seq mismatch.
   */
  int fw_upload_write_chunk(uint32_t seq, const uint8_t *data, uint32_t len);

  /**
   * @brief Verify the staging buffer CRC32 against the expected CRC.
   *
   * Reads the entire staging buffer from W25Q64, computes CRC32, and
   * compares against expected_crc from fw_upload_start().
   *
   * @return 0 on match, -1 on mismatch, -2 if not in COMPLETE state.
   */
  int fw_upload_verify(void);

  /**
   * @brief Commit the staged firmware to the inactive internal flash slot.
   *
   * Programs the inactive slot from W25Q64 staging via flash_range_erase()
   * and flash_range_program().  This operation is BLOCKING and must be
   * called with interrupts disabled or inside a critical section.
   *
   * @return 0 on success, -1 on failure.
   */
  int fw_upload_commit(void);

  /**
   * @brief Abort the current upload and return to IDLE.
   *
   * Does NOT erase the W25Q64 staging area (preserved for diagnostics).
   */
  void fw_upload_abort(void);

  /**
   * @brief Get the current upload status.
   *
   * @param[out] status  Filled with current upload state and counters.
   */
  void fw_upload_get_status(fw_upload_status_t *status);

  /**
   * @brief Check whether an upload is in progress.
   *
   * @return true if state is RECEIVING.
   */
  bool fw_upload_is_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_UPLOAD_H */
