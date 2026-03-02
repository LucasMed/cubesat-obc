/**
 * @file test_data_layer.c
 * @brief PR-2 gate: Data Layer Abstraction correctness checks.
 *
 * Verifies:
 *   1. data_layer_init() zeroes the snapshot (except default mode/energy)
 *   2. data_layer_write_imu() / data_layer_read() round-trip (values in rad)
 *   3. data_layer_write_temp() round-trip
 *   4. data_layer_set_sensor_avail() reflected in snapshot
 *   5. data_layer_set_flight_mode() / data_layer_get_flight_mode()
 *   6. data_layer_set_energy_state() / data_layer_get_energy_state()
 *   7. seq counter increments on every write, not on reads
 *   8. system_state shim: system_state_get() returns same sensor data
 *   9. Angles stored as-is in radians (no hidden conversion)
 */

#include "../../include/data_layer.h"
#include "../../include/system_state.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int g_failures = 0;

#define CHECK(cond, msg)                                           \
    do                                                             \
    {                                                              \
        if (!(cond))                                               \
        {                                                          \
            printf("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
            g_failures++;                                          \
        }                                                          \
    } while (0)

#define CHECK_FLOAT(a, b, msg)                                         \
    do                                                                 \
    {                                                                  \
        if (fabsf((a) - (b)) > 1e-6f)                                  \
        {                                                              \
            printf("FAIL [%s:%d]: %s  (got %.8f, expected %.8f)\n",    \
                   __FILE__, __LINE__, msg, (double)(a), (double)(b)); \
            g_failures++;                                              \
        }                                                              \
    } while (0)

/* ------------------------------------------------------------------ */
/* Test 1: init state                                                  */
/* ------------------------------------------------------------------ */

static void test_init(void)
{
    data_layer_init();

    dl_snapshot_t snap;
    data_layer_read(&snap);

    CHECK(snap.state.attitude[0] == 0.0f, "attitude[0] must be 0 after init");
    CHECK(snap.state.attitude[1] == 0.0f, "attitude[1] must be 0 after init");
    CHECK(snap.state.attitude[2] == 0.0f, "attitude[2] must be 0 after init");
    CHECK(snap.state.rates[0] == 0.0f, "rates[0] must be 0 after init");
    CHECK(snap.state.temp == 0.0f, "temp must be 0 after init");
    CHECK(snap.state.imu_valid == false, "imu_valid must be false after init");
    CHECK(snap.state.temp_valid == false, "temp_valid must be false after init");

    /* Default mode/energy per spec */
    CHECK(snap.mode == FM_BOOT, "mode must be FM_BOOT after init");
    CHECK(snap.energy == ENERGY_NOMINAL, "energy must be ENERGY_NOMINAL after init");
    CHECK(snap.seq == 0, "seq must be 0 after init");

    printf("test_init: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 2: IMU write/read round-trip                                   */
/* ------------------------------------------------------------------ */

static void test_imu_rw(void)
{
    data_layer_init();

    /* Values in radians as per SPEC-2-DLA §2.4 */
    const float att[3] = {(float)(M_PI / 6.0),      /* 30 deg in rad */
                          (float)(M_PI / 4.0),      /* 45 deg in rad */
                          (float)(M_PI / 3.0)};     /* 60 deg in rad */
    const float rates[3] = {0.01f, -0.02f, 0.005f}; /* rad/s */

    data_layer_write_imu(att, rates);

    dl_snapshot_t snap;
    data_layer_read(&snap);

    CHECK_FLOAT(snap.state.attitude[0], att[0], "attitude[0] round-trip");
    CHECK_FLOAT(snap.state.attitude[1], att[1], "attitude[1] round-trip");
    CHECK_FLOAT(snap.state.attitude[2], att[2], "attitude[2] round-trip");
    CHECK_FLOAT(snap.state.rates[0], rates[0], "rates[0] round-trip");
    CHECK_FLOAT(snap.state.rates[1], rates[1], "rates[1] round-trip");
    CHECK_FLOAT(snap.state.rates[2], rates[2], "rates[2] round-trip");

    CHECK(snap.state.imu_valid == true, "imu_valid must be true after write");
    CHECK(snap.seq == 1, "seq must be 1 after one IMU write");

    printf("test_imu_rw: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 3: rad storage — values passed in are stored unchanged         */
/* ------------------------------------------------------------------ */

static void test_radians_storage(void)
{
    data_layer_init();

    /* 1 radian is NOT a round number in degrees; verify no conversion
       occurs inside the DLA itself. */
    const float att[3] = {1.0f, 2.0f, 3.0f};
    const float rates[3] = {0.1f, 0.2f, 0.3f};

    data_layer_write_imu(att, rates);

    dl_snapshot_t snap;
    data_layer_read(&snap);

    CHECK_FLOAT(snap.state.attitude[0], 1.0f, "DLA must store 1.0 rad without conversion");
    CHECK_FLOAT(snap.state.attitude[1], 2.0f, "DLA must store 2.0 rad without conversion");
    CHECK_FLOAT(snap.state.attitude[2], 3.0f, "DLA must store 3.0 rad without conversion");

    printf("test_radians_storage: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 4: temperature write/read round-trip                           */
/* ------------------------------------------------------------------ */

static void test_temp_rw(void)
{
    data_layer_init();
    data_layer_write_temp(42.5f);

    dl_snapshot_t snap;
    data_layer_read(&snap);

    CHECK_FLOAT(snap.state.temp, 42.5f, "temp round-trip");
    CHECK(snap.state.temp_valid == true, "temp_valid must be true after write");
    CHECK(snap.seq == 1, "seq must be 1 after one temp write");

    printf("test_temp_rw: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 5: sensor availability flags                                   */
/* ------------------------------------------------------------------ */

static void test_sensor_avail(void)
{
    data_layer_init();
    data_layer_set_sensor_avail(true, false);

    dl_snapshot_t snap;
    data_layer_read(&snap);

    CHECK(snap.state.imu_available == true, "imu_available must be true");
    CHECK(snap.state.temp_available == false, "temp_available must be false");

    data_layer_set_sensor_avail(false, true);
    data_layer_read(&snap);
    CHECK(snap.state.imu_available == false, "imu_available toggled to false");
    CHECK(snap.state.temp_available == true, "temp_available toggled to true");

    printf("test_sensor_avail: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 6: flight mode                                                 */
/* ------------------------------------------------------------------ */

static void test_flight_mode(void)
{
    data_layer_init();

    CHECK(data_layer_get_flight_mode() == FM_BOOT, "initial mode must be FM_BOOT");

    data_layer_set_flight_mode(FM_NOMINAL);
    CHECK(data_layer_get_flight_mode() == FM_NOMINAL, "mode must update to FM_NOMINAL");

    dl_snapshot_t snap;
    data_layer_read(&snap);
    CHECK(snap.mode == FM_NOMINAL, "snapshot.mode must equal FM_NOMINAL");

    printf("test_flight_mode: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 7: energy state                                                */
/* ------------------------------------------------------------------ */

static void test_energy_state(void)
{
    data_layer_init();

    CHECK(data_layer_get_energy_state() == ENERGY_NOMINAL, "initial energy must be ENERGY_NOMINAL");

    data_layer_set_energy_state(ENERGY_LOW);
    CHECK(data_layer_get_energy_state() == ENERGY_LOW, "energy must update to ENERGY_LOW");

    data_layer_set_energy_state(ENERGY_CRITICAL);
    dl_snapshot_t snap;
    data_layer_read(&snap);
    CHECK(snap.energy == ENERGY_CRITICAL, "snapshot.energy must equal ENERGY_CRITICAL");

    printf("test_energy_state: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 8: seq counter                                                 */
/* ------------------------------------------------------------------ */

static void test_seq_counter(void)
{
    data_layer_init();
    CHECK(data_layer_get_seq() == 0, "seq must start at 0");

    const float att[3] = {0.0f, 0.0f, 0.0f};
    const float rates[3] = {0.0f, 0.0f, 0.0f};

    data_layer_write_imu(att, rates);
    CHECK(data_layer_get_seq() == 1, "seq must be 1 after imu write");

    data_layer_write_temp(25.0f);
    CHECK(data_layer_get_seq() == 2, "seq must be 2 after temp write");

    data_layer_set_flight_mode(FM_SAFE);
    CHECK(data_layer_get_seq() == 3, "seq must be 3 after mode write");

    data_layer_set_energy_state(ENERGY_EMERGENCY);
    CHECK(data_layer_get_seq() == 4, "seq must be 4 after energy write");

    /* Reads must NOT increment seq */
    dl_snapshot_t snap;
    data_layer_read(&snap);
    CHECK(data_layer_get_seq() == 4, "seq must not increment on read");
    (void)data_layer_get_flight_mode();
    (void)data_layer_get_energy_state();
    CHECK(data_layer_get_seq() == 4, "seq must not increment on accessor reads");

    printf("test_seq_counter: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 9: system_state shim consistency                               */
/* ------------------------------------------------------------------ */

static void test_system_state_shim(void)
{
    data_layer_init();

    const float att[3] = {0.5f, 1.0f, 1.5f};
    const float rates[3] = {0.01f, 0.02f, 0.03f};
    system_state_set_imu(att, rates);
    system_state_set_temp(37.0f);

    /* DLA read must see the same data written via legacy shim */
    dl_snapshot_t snap;
    data_layer_read(&snap);
    CHECK_FLOAT(snap.state.attitude[0], 0.5f, "shim: attitude[0]");
    CHECK_FLOAT(snap.state.attitude[1], 1.0f, "shim: attitude[1]");
    CHECK_FLOAT(snap.state.rates[2], 0.03f, "shim: rates[2]");
    CHECK_FLOAT(snap.state.temp, 37.0f, "shim: temp");

    /* Legacy system_state_get() must also reflect DLA writes */
    data_layer_set_flight_mode(FM_DETUMBLE); /* mode change */
    const float att2[3] = {2.0f, 2.1f, 2.2f};
    const float rates2[3] = {0.0f, 0.0f, 0.0f};
    data_layer_write_imu(att2, rates2);

    system_state_t state;
    system_state_get(&state);
    CHECK_FLOAT(state.attitude[0], 2.0f, "legacy get: attitude[0] after DLA write");

    printf("test_system_state_shim: OK\n");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
    test_init();
    test_imu_rw();
    test_radians_storage();
    test_temp_rw();
    test_sensor_avail();
    test_flight_mode();
    test_energy_state();
    test_seq_counter();
    test_system_state_shim();

    if (g_failures == 0)
    {
        printf("All Data Layer checks passed.\n");
        return 0;
    }
    printf("%d Data Layer check(s) FAILED.\n", g_failures);
    return 1;
}
