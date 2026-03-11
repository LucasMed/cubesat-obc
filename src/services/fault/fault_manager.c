/**
 * @file fault_manager.c
 * @brief Fault Manager implementation.
 *
 * Maintains a flat table of up to FAULT_TABLE_CAPACITY fault entries.
 * Each entry is keyed by its 16-bit fault ID (from fault_ids.h).
 * Table size is generous enough to hold every defined ID plus a few
 * spare slots for future additions — linear search over 32 entries is
 * negligible at the call rates expected.
 *
 * Threading:
 *   - PICO_BUILD: taskENTER_CRITICAL / taskEXIT_CRITICAL (ISR-safe).
 *   - Host build: no locking (tests are single-threaded).
 *
 * Fault ageing policy (SPEC-2-FMM §6):
 *   - FAULT_LEVEL_WARNING: auto-cleared after WARN_AUTO_CLEAR_TICKS ticks
 *     if not re-raised.
 *   - FAULT_LEVEL_ERROR / CRITICAL: never auto-cleared; require an
 *     explicit fault_clear() call.
 *
 * Spec ref: SPEC-2-FMM v1.1 §4–6, SPEC-2 v2.0 §4.2
 */

#include "fault_manager.h"

#include "flight_mode.h"

#include "FreeRTOS.h"
#include "task.h"
#include "config.h"
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */

/** Maximum number of unique fault IDs tracked simultaneously. */
#define FAULT_TABLE_CAPACITY 32u

/**
 * Number of fault_manager_tick() calls (1 Hz) before an unacknowledged
 * WARNING fault is automatically cleared.
 */
#define WARN_AUTO_CLEAR_TICKS 30u

/* ------------------------------------------------------------------ */
/* Internal table entry                                                */
/* ------------------------------------------------------------------ */

typedef struct
{
  fault_event_t event;  /**< Public-facing fault record          */
  uint32_t tick_raised; /**< Tick count when fault was first set */
} fault_entry_t;

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */

static fault_entry_t g_table[FAULT_TABLE_CAPACITY];
static uint32_t g_tick;           /**< Monotonic tick counter, incremented by _tick() */
static TaskHandle_t g_hm_task = NULL; /**< Health Monitor task handle for FDIR signaling */

/* ------------------------------------------------------------------ */
/* Platform helpers                                                    */
/* ------------------------------------------------------------------ */

static uint32_t get_tick_ms(void)
{
#ifdef PICO_BUILD
  return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
  return g_tick * 1000u; /* 1 Hz tick → ms */
#endif
}

static void fm_lock(void)
{
#ifdef PICO_BUILD
  taskENTER_CRITICAL();
#endif
}

static void fm_unlock(void)
{
#ifdef PICO_BUILD
  taskEXIT_CRITICAL();
#endif
}

/* Weak forward declaration resolved by flight_mode_manager.c (PR-3). */
extern void fmm_force_safe(void);

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/**
 * Find the table slot for a given fault ID, or the first empty slot.
 * Returns FAULT_TABLE_CAPACITY if the table is full and ID not found.
 */
static uint32_t find_slot(uint16_t id)
{
  uint32_t first_empty = FAULT_TABLE_CAPACITY;
  for (uint32_t i = 0; i < FAULT_TABLE_CAPACITY; i++)
  {
    if (g_table[i].event.id == id)
    {
      return i; /* found existing entry */
    }
    if (first_empty == FAULT_TABLE_CAPACITY && g_table[i].event.id == 0u)
    {
      first_empty = i; /* remember first unused slot */
    }
  }
  return first_empty; /* caller checks against FAULT_TABLE_CAPACITY */
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void fault_manager_init(void)
{
  fm_lock();
  for (uint32_t i = 0; i < FAULT_TABLE_CAPACITY; i++)
  {
    fault_entry_t zero = {0};
    g_table[i] = zero;
  }
  g_tick = 0u;
  fm_unlock();
}

void fault_report(uint16_t id, fault_level_t level)
{
  if (id == 0u || level == FAULT_LEVEL_NONE)
  {
    return; /* 0 is reserved as "empty slot" sentinel */
  }

  fm_lock();

  uint32_t slot = find_slot(id);
  if (slot == FAULT_TABLE_CAPACITY)
  {
    fm_unlock();
    return; /* table full — drop silently (should never happen in practice) */
  }

  fault_entry_t *e = &g_table[slot];

  if (e->event.id == 0u)
  {
    /* New entry */
    e->event.id = id;
    e->event.count = 1u;
    e->event.timestamp_ms = get_tick_ms();
    e->tick_raised = g_tick;
  }
  else
  {
    /* Existing entry — update level if escalating, always bump count */
    e->event.count++;
    if ((uint8_t)level > (uint8_t)e->event.level)
    {
      e->tick_raised = g_tick; /* reset age on escalation */
    }
  }

  e->event.level = level;
  e->event.active = true;

  fm_unlock();

  /* CRITICAL: trigger safe mode transition.
   * We use task notifications to ensure the mode change happens in the 
   * HealthMonitorTask context (ISR-safe).
   * If no task handle is set, we fall back to a direct call (legacy/tests). */
  if (level >= FAULT_LEVEL_CRITICAL)
  {
    if (g_hm_task != NULL)
    {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xTaskNotifyFromISR(g_hm_task, HM_NOTIFY_FAULT_CRITICAL, eSetBits, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
      fmm_force_safe();
    }
  }
}

void fault_clear(uint16_t id)
{
  fm_lock();
  uint32_t slot = find_slot(id);
  if (slot < FAULT_TABLE_CAPACITY && g_table[slot].event.id == id)
  {
    g_table[slot].event.active = false;
    g_table[slot].event.level = FAULT_LEVEL_NONE;
  }
  fm_unlock();
}

bool fault_is_active(uint16_t id)
{
  fm_lock();
  uint32_t slot = find_slot(id);
  bool active =
      (slot < FAULT_TABLE_CAPACITY) && (g_table[slot].event.id == id) && g_table[slot].event.active;
  fm_unlock();
  return active;
}

fault_level_t fault_get_highest_level(void)
{
  fault_level_t highest = FAULT_LEVEL_NONE;
  fm_lock();
  for (uint32_t i = 0; i < FAULT_TABLE_CAPACITY; i++)
  {
    if (g_table[i].event.active && (uint8_t)g_table[i].event.level > (uint8_t)highest)
    {
      highest = g_table[i].event.level;
    }
  }
  fm_unlock();
  return highest;
}

void fault_manager_tick(void)
{
  fm_lock();
  g_tick++;
  for (uint32_t i = 0; i < FAULT_TABLE_CAPACITY; i++)
  {
    fault_entry_t *e = &g_table[i];
    if (!e->event.active || e->event.level != FAULT_LEVEL_WARNING)
    {
      continue;
    }
    /* Auto-clear WARNING faults that have not been re-raised */
    if ((g_tick - e->tick_raised) >= WARN_AUTO_CLEAR_TICKS)
    {
      e->event.active = false;
      e->event.level = FAULT_LEVEL_NONE;
    }
  }
  fm_unlock();
}

bool fault_get_event(uint16_t id, fault_event_t *out)
{
  if (!out || id == 0u)
  {
    return false;
  }
  fm_lock();
  uint32_t slot = find_slot(id);
  if (slot >= FAULT_TABLE_CAPACITY || g_table[slot].event.id != id)
  {
    fm_unlock();
    return false;
  }
  *out = g_table[slot].event;
  fm_unlock();
  return true;
}

void fault_manager_set_hm_task_handle(void *h_health)
{
  fm_lock();
  g_hm_task = (TaskHandle_t)h_health;
  fm_unlock();
}
