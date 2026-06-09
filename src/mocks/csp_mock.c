/**
 * @file csp_mock.c
 * @brief CSP mock implementation for unit tests.
 *
 * Provides configurable mock implementations of key CSP socket functions
 * that can be linked into unit test executables in place of the real
 * libcsp.  See csp_mock.h for usage.
 */
#include "mocks/csp_mock.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Mock state                                                          */
/* ------------------------------------------------------------------ */

csp_conn_t *csp_mock_accept_return = NULL;
csp_packet_t *csp_mock_read_return = NULL;
bool csp_mock_read_keep_returning = false;

int csp_mock_bind_calls = 0;
int csp_mock_listen_calls = 0;
int csp_mock_accept_calls = 0;
int csp_mock_read_calls = 0;
int csp_mock_send_calls = 0;
int csp_mock_close_calls = 0;
int csp_mock_buffer_free_calls = 0;
int csp_mock_buffer_get_calls = 0;

uint8_t csp_mock_last_send_data[256];
size_t csp_mock_last_send_len = 0;

/* ------------------------------------------------------------------ */
/* Mock implementations                                                */
/* ------------------------------------------------------------------ */

int mock_csp_bind(csp_socket_t *sock, uint16_t port)
{
  (void)sock;
  (void)port;
  csp_mock_bind_calls++;
  return 0;
}

int mock_csp_listen(csp_socket_t *sock, size_t backlog)
{
  (void)sock;
  (void)backlog;
  csp_mock_listen_calls++;
  return 0;
}

csp_conn_t *mock_csp_accept(csp_socket_t *sock, uint32_t timeout)
{
  (void)sock;
  (void)timeout;
  csp_mock_accept_calls++;
  return csp_mock_accept_return;
}

csp_packet_t *mock_csp_read(csp_conn_t *conn, uint32_t timeout)
{
  (void)conn;
  (void)timeout;
  csp_mock_read_calls++;
  csp_packet_t *pkt = csp_mock_read_return;
  if (!csp_mock_read_keep_returning)
  {
    csp_mock_read_return = NULL; /* return only once */
  }
  return pkt;
}

void mock_csp_send(csp_conn_t *conn, csp_packet_t *packet)
{
  (void)conn;
  csp_mock_send_calls++;
  if (packet != NULL)
  {
    size_t copy_len = packet->length;
    if (copy_len > sizeof(csp_mock_last_send_data))
      copy_len = sizeof(csp_mock_last_send_data);
    memcpy(csp_mock_last_send_data, packet->data, copy_len);
    csp_mock_last_send_len = copy_len;
    free(packet);
  }
}

void mock_csp_close(csp_conn_t *conn)
{
  (void)conn;
  csp_mock_close_calls++;
}

int mock_csp_buffer_free(void *packet)
{
  csp_mock_buffer_free_calls++;
  free(packet);
  return 0;
}

csp_packet_t *mock_csp_buffer_get(size_t size)
{
  (void)size;
  csp_mock_buffer_get_calls++;
  csp_packet_t *pkt = (csp_packet_t *)calloc(1, sizeof(csp_packet_t) + 256);
  if (pkt != NULL)
  {
    pkt->length = 0;
  }
  return pkt;
}

int mock_csp_conn_src(const csp_conn_t *conn)
{
  (void)conn;
  return 1; /* simulated GN address */
}

void mock_csp_sendto(uint8_t prio, uint16_t dest, uint8_t dport, uint8_t sport, uint32_t opts,
                     csp_packet_t *packet)
{
  (void)prio;
  (void)dest;
  (void)dport;
  (void)sport;
  (void)opts;
  csp_mock_send_calls++;
  if (packet != NULL)
  {
    size_t copy_len = packet->length;
    if (copy_len > sizeof(csp_mock_last_send_data))
      copy_len = sizeof(csp_mock_last_send_data);
    memcpy(csp_mock_last_send_data, packet->data, copy_len);
    csp_mock_last_send_len = copy_len;
    free(packet);
  }
}

/* ------------------------------------------------------------------ */
/* Reset                                                               */
/* ------------------------------------------------------------------ */

void csp_mock_reset(void)
{
  csp_mock_accept_return = NULL;
  csp_mock_read_return = NULL;
  csp_mock_read_keep_returning = false;

  csp_mock_bind_calls = 0;
  csp_mock_listen_calls = 0;
  csp_mock_accept_calls = 0;
  csp_mock_read_calls = 0;
  csp_mock_send_calls = 0;
  csp_mock_close_calls = 0;
  csp_mock_buffer_free_calls = 0;
  csp_mock_buffer_get_calls = 0;

  memset(csp_mock_last_send_data, 0, sizeof(csp_mock_last_send_data));
  csp_mock_last_send_len = 0;
}
