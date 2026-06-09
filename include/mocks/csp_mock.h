/**
 * @file csp_mock.h
 * @brief CSP mock interface for unit tests.
 *
 * Provides configurable mock callbacks for CSP socket operations:
 *   - csp_bind / csp_listen / csp_close
 *   - csp_accept (returns a mock connection or NULL for timeout)
 *   - csp_read   (returns a mock packet or NULL for timeout)
 *   - csp_send   (captures sent packet data)
 *   - csp_buffer_free / csp_buffer_get
 *
 * Usage:
 *   1. Include this header in the test file.
 *   2. Define CSP_MOCK before including the source-under-test.
 *   3. Call csp_mock_reset() in setUp() or reset_all().
 *   4. Configure accept_return / read_return / send_callback as needed.
 *
 * In the source-under-test, add macro renames under #ifdef CSP_MOCK:
 *   #define csp_bind        mock_csp_bind
 *   #define csp_listen      mock_csp_listen
 *   #define csp_accept      mock_csp_accept
 *   #define csp_read        mock_csp_read
 *   #define csp_send        mock_csp_send
 *   #define csp_close       mock_csp_close
 *   #define csp_buffer_free mock_csp_buffer_free
 *   #define csp_buffer_get  mock_csp_buffer_get
 *   #define csp_conn_src    mock_csp_conn_src
 */

#ifndef CSP_MOCK_H
#define CSP_MOCK_H

#include <csp/csp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Configurable mock state                                              */
  /* ------------------------------------------------------------------ */

  /** Set to a non-NULL connection pointer to make csp_accept return it. */
  extern csp_conn_t *csp_mock_accept_return;

  /**
   * Set to a non-NULL packet pointer to make csp_read return it.
   * The mock does NOT take ownership — the caller must not free it
   * unless the real code path calls csp_send or csp_buffer_free.
   */
  extern csp_packet_t *csp_mock_read_return;

  /**
   * After a read returns a packet, the next call will return NULL
   * (simulating end-of-stream). Set to true for multi-packet reads.
   */
  extern bool csp_mock_read_keep_returning;

  /** Call counters — incremented on each mock call. */
  extern int csp_mock_bind_calls;
  extern int csp_mock_listen_calls;
  extern int csp_mock_accept_calls;
  extern int csp_mock_read_calls;
  extern int csp_mock_send_calls;
  extern int csp_mock_close_calls;
  extern int csp_mock_buffer_free_calls;
  extern int csp_mock_buffer_get_calls;

  /** Last sent packet data — copied before free so tests can inspect it. */
  extern uint8_t csp_mock_last_send_data[256];
  extern size_t csp_mock_last_send_len;

  /* ------------------------------------------------------------------ */
  /* Mock implementations                                                */
  /* ------------------------------------------------------------------ */

  /** csp_bind mock — no-op, returns 0. */
  int mock_csp_bind(csp_socket_t *sock, uint16_t port);

  /** csp_listen mock — no-op, returns 0. */
  int mock_csp_listen(csp_socket_t *sock, size_t backlog);

  /** csp_accept mock — returns csp_mock_accept_return (set to NULL for timeout). */
  csp_conn_t *mock_csp_accept(csp_socket_t *sock, uint32_t timeout);

  /** csp_read mock — returns csp_mock_read_return once, then NULL. */
  csp_packet_t *mock_csp_read(csp_conn_t *conn, uint32_t timeout);

  /** csp_send mock — copies packet data to csp_mock_last_send_data, frees packet. */
  void mock_csp_send(csp_conn_t *conn, csp_packet_t *packet);

  /** csp_close mock — no-op. */
  void mock_csp_close(csp_conn_t *conn);

  /** csp_buffer_free mock — frees the packet. */
  int mock_csp_buffer_free(void *packet);

  /** csp_buffer_get mock — allocates a zeroed packet. */
  csp_packet_t *mock_csp_buffer_get(size_t size);

  /** csp_conn_src mock — returns 1 (simulated GN address). */
  int mock_csp_conn_src(const csp_conn_t *conn);

  /** csp_sendto mock — captures packet data. */
  void mock_csp_sendto(uint8_t prio, uint16_t dest, uint8_t dport, uint8_t sport, uint32_t opts,
                       csp_packet_t *packet);

  /* ------------------------------------------------------------------ */
  /* Reset helper                                                         */
  /* ------------------------------------------------------------------ */

  /** Reset all mock state to defaults. Call in setUp() or reset_all(). */
  void csp_mock_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* CSP_MOCK_H */
