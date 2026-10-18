// Núcleo de minado del NerdMiner_v2 (mining.cpp, MIT © Bitmaker), recortado para el badge:
// - solo ESP32-S3: el trabajador de SHA-256 por hardware (sin el de software, que se comería la pantalla)
// - sin pantallas, sin WiFiManager, sin estadísticas en NVS
// - con start()/stop() y la configuración que le pasa el badge
// Lo que hace cada trozo está explicado en el original; aquí se deja igual salvo lo marcado "badge:".
#include <Arduino.h>
#include <WiFi.h>
#include <mutex>
#include <list>
#include <map>
#include <memory>
#include "nerdminer.h"
#include "nerdSHA256plus.h"
#include "stratum.h"
#include "mining.h"
#include "utils.h"
#include <sha/sha_dma.h>
#include <hal/sha_hal.h>
#include <hal/sha_ll.h>

namespace nerdminer {

#define NONCE_PER_JOB_HW (16 * 1024)

// ---------------------------------------------------------------- estado (badge: en vez de Settings y mMonitor)
static Config        cfg;
static bool        (*isOnline)() = nullptr;
static void        (*socketOpen)(bool) = nullptr;   // badge: para que net/radio sepa cuándo puede apagar la WiFi
static volatile bool wantRunning = false;
static bool          tasksStarted = false;

static volatile uint32_t hashes = 0;          // se va sumando; stats() lo convierte en kH/s
static volatile uint32_t shares = 0, valids = 0, templates = 0, lastShareAt = 0;
static volatile uint32_t accepted = 0, rejected = 0, minedMs = 0;
static volatile double   poolDiff = 0;
static volatile double   bestDiff = 0.0;
static volatile bool     poolReady = false;

static WiFiClient        client;
static miner_data        mMiner;
static mining_subscribe  mWorker;
static mining_job        mJob;
static bool              isMinerSuscribed = false;
static uint32_t          mLastTXtoPool = 0;

struct JobRequest {
  uint32_t id, nonce_start, nonce_count;
  double   difficulty;
  uint8_t  sha_buffer[128];
  uint32_t midstate[8];
  uint32_t bake[16];
};
struct JobResult {
  uint32_t id, nonce, nonce_count;
  double   difficulty;
  uint8_t  hash[32];
};
struct Submition { double diff; bool is32bit; bool isValid; };

static std::mutex s_job_mutex;
static std::list<std::shared_ptr<JobRequest>> s_job_request_list_hw;
static std::list<std::shared_ptr<JobResult>>  s_job_result_list;
static volatile uint8_t s_working_current_job_id = 0xFF;

static void JobPush(uint32_t id, uint32_t nonce_start, uint32_t nonce_count, double difficulty,
                    const uint8_t* sha_buffer, const uint32_t* midstate, const uint32_t* bake) {
  auto job = std::make_shared<JobRequest>();
  job->id = id;
  job->nonce_start = nonce_start;
  job->nonce_count = nonce_count;
  job->difficulty = difficulty;
  memcpy(job->sha_buffer, sha_buffer, sizeof(job->sha_buffer));
  memcpy(job->midstate, midstate, sizeof(job->midstate));
  memcpy(job->bake, bake, sizeof(job->bake));
  s_job_request_list_hw.push_back(job);
}

static void MiningJobStop(uint32_t& job_pool, std::map<uint32_t, std::shared_ptr<Submition>>& submition_map) {
  {
    std::lock_guard<std::mutex> lock(s_job_mutex);
    s_job_result_list.clear();
    s_job_request_list_hw.clear();
  }
  s_working_current_job_id = 0xFF;
  job_pool = 0xFFFFFFFF;
  submition_map.clear();
  poolReady = false;
}

static bool checkPoolConnection() {
  if (client.connected()) return true;
  isMinerSuscribed = false;
  Serial.printf("[miner] conectando a %s:%u\n", cfg.poolHost.c_str(), cfg.poolPort);
  return client.connect(cfg.poolHost.c_str(), cfg.poolPort);
}

// ---------------------------------------------------------------- trabajador de stratum (de runStratumWorker)
static void stratumTask(void*) {
  std::map<uint32_t, std::shared_ptr<Submition>> s_submition_map;
  double currentPoolDifficulty = DEFAULT_DIFFICULTY;
  uint32_t nonce_pool = 0, job_pool = 0xFFFFFFFF, last_job_time = millis();
  uint32_t hw_midstate[8], diget_mid[8], bake[16];

  for (;;) {
    // badge: parado o sin internet -> soltar el pool y esperar
    if (!wantRunning || !isOnline || !isOnline()) {
      client.stop();
      if (socketOpen) socketOpen(false);
      isMinerSuscribed = false;
      MiningJobStop(job_pool, s_submition_map);
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    if (socketOpen) socketOpen(true);
    if (!checkPoolConnection()) {
      MiningJobStop(job_pool, s_submition_map);
      vTaskDelay(pdMS_TO_TICKS(10000));
      continue;
    }
    if (!isMinerSuscribed) {
      mWorker = init_mining_subscribe();
      if (!tx_mining_subscribe(client, mWorker)) {
        client.stop();
        MiningJobStop(job_pool, s_submition_map);
        continue;
      }
      strncpy(mWorker.wName, cfg.address.c_str(), sizeof(mWorker.wName) - 1);
      strncpy(mWorker.wPass, cfg.password.c_str(), sizeof(mWorker.wPass) - 1);
      tx_mining_auth(client, mWorker.wName, mWorker.wPass);
      tx_suggest_difficulty(client, currentPoolDifficulty);
      isMinerSuscribed = true;
      mLastTXtoPool = last_job_time = millis();
    }

    // Mantener viva la conexión y, si el pool calla 10 min, empezar de nuevo
    if (millis() - mLastTXtoPool > KEEPALIVE_TIME_ms) {
      mLastTXtoPool = millis();
      tx_suggest_difficulty(client, DEFAULT_DIFFICULTY);
    }
    if (millis() - last_job_time > 10 * 60 * 1000) {
      client.stop();
      isMinerSuscribed = false;
      MiningJobStop(job_pool, s_submition_map);
      continue;
    }

    // Lo que manda el pool
    while (client.connected() && client.available()) {
      String line = client.readStringUntil('\n');
      switch (parse_mining_method(line)) {
        case MINING_NOTIFY:
          if (parse_mining_notify(line, mJob)) {
            {
              std::lock_guard<std::mutex> lock(s_job_mutex);
              s_job_request_list_hw.clear();
            }
            templates++;
            job_pool++;
            s_working_current_job_id = job_pool & 0xFF;
            last_job_time = mLastTXtoPool = millis();

            mMiner = calculateMiningData(mWorker, mJob);
            memset(mMiner.bytearray_blockheader + 80, 0, 128 - 80);
            mMiner.bytearray_blockheader[80] = 0x80;
            mMiner.bytearray_blockheader[126] = 0x02;
            mMiner.bytearray_blockheader[127] = 0x80;
            nerd_mids(diget_mid, mMiner.bytearray_blockheader);
            nerd_sha256_bake(diget_mid, mMiner.bytearray_blockheader + 64, bake);

            esp_sha_acquire_hardware();
            sha_hal_hash_block(SHA2_256, mMiner.bytearray_blockheader, 64 / 4, true);
            sha_hal_read_digest(SHA2_256, hw_midstate);
            esp_sha_release_hardware();

            nonce_pool = 0xDA54E700;   // el nonce 0 no vale: se empieza en uno cualquiera
            {
              std::lock_guard<std::mutex> lock(s_job_mutex);
              for (int i = 0; i < 4; ++i) {
                JobPush(job_pool, nonce_pool, NONCE_PER_JOB_HW, currentPoolDifficulty, mMiner.bytearray_blockheader, hw_midstate, bake);
                nonce_pool += NONCE_PER_JOB_HW;
              }
            }
            poolReady = true;
          } else {
            client.stop();
            isMinerSuscribed = false;
            MiningJobStop(job_pool, s_submition_map);
          }
          break;
        case MINING_SET_DIFFICULTY:
          parse_mining_set_difficulty(line, currentPoolDifficulty);
          poolDiff = currentPoolDifficulty;
          break;
        case STRATUM_SUCCESS: {
          auto it = s_submition_map.find(parse_extract_id(line));
          if (it != s_submition_map.end()) {
            accepted++;
            lastShareAt = millis();
            if (it->second->is32bit) shares++;
            if (it->second->isValid) { valids++; Serial.println("[miner] ¡BLOQUE ENCONTRADO!"); }
            s_submition_map.erase(it);
          }
          break;
        }
        case STRATUM_PARSE_ERROR: {
          auto it = s_submition_map.find(parse_extract_id(line));
          if (it != s_submition_map.end()) { rejected++; s_submition_map.erase(it); }
          break;
        }
        default: break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    {
      static uint32_t lastTick = millis();             // badge: tiempo minando con trabajo del pool
      const uint32_t now = millis();
      if (poolReady) minedMs += now - lastTick;
      lastTick = now;
    }

    std::list<std::shared_ptr<JobResult>> results;
    if (job_pool != 0xFFFFFFFF) {
      std::lock_guard<std::mutex> lock(s_job_mutex);
      results.splice(results.end(), s_job_result_list);
      while (s_job_request_list_hw.size() < 4) {
        JobPush(job_pool, nonce_pool, NONCE_PER_JOB_HW, currentPoolDifficulty, mMiner.bytearray_blockheader, hw_midstate, bake);
        nonce_pool += NONCE_PER_JOB_HW;
      }
    }

    for (auto& res : results) {
      hashes += res->nonce_count;
      if (res->difficulty > currentPoolDifficulty && job_pool == res->id && res->nonce != 0xFFFFFFFF) {
        if (!client.connected()) break;
        unsigned long submit_id = 0;
        tx_mining_submit(client, mWorker, mJob, res->nonce, submit_id);
        mLastTXtoPool = millis();
        auto s = std::make_shared<Submition>();
        s->diff = res->difficulty;
        s->is32bit = res->hash[29] == 0 && res->hash[28] == 0;
        s->isValid = s->is32bit && checkValid(res->hash, mMiner.bytearray_target);
        s_submition_map.insert(std::make_pair(submit_id, s));
        if (s_submition_map.size() > 32) s_submition_map.erase(s_submition_map.begin());
      }
    }
  }
}

// ---------------------------------------------------------------- trabajador de SHA-256 por hardware (de minerWorkerHw, S3)

static inline void fill_text_block(const void* input_text, uint32_t nonce) {
  const uint32_t* data_words = (const uint32_t*)input_text;
  uint32_t* reg = (uint32_t*)(SHA_TEXT_BASE);
  REG_WRITE(&reg[0], data_words[0]);
  REG_WRITE(&reg[1], data_words[1]);
  REG_WRITE(&reg[2], data_words[2]);
  REG_WRITE(&reg[3], nonce);
  REG_WRITE(&reg[4], 0x00000080);
  for (int i = 5; i < 15; i++) REG_WRITE(&reg[i], 0x00000000);
  REG_WRITE(&reg[15], 0x80020000);
}

static inline void fill_text_block_inter() {
  uint32_t* reg = (uint32_t*)(SHA_TEXT_BASE);
  DPORT_INTERRUPT_DISABLE();
  for (int i = 0; i < 8; i++) REG_WRITE(&reg[i], DPORT_SEQUENCE_REG_READ(SHA_H_BASE + i * 4));
  DPORT_INTERRUPT_RESTORE();
  REG_WRITE(&reg[8], 0x00000080);
  for (int i = 9; i < 15; i++) REG_WRITE(&reg[i], 0x00000000);
  REG_WRITE(&reg[15], 0x00010000);
}

static inline bool read_digest_if(void* ptr) {
  DPORT_INTERRUPT_DISABLE();
  const uint32_t last = DPORT_SEQUENCE_REG_READ(SHA_H_BASE + 7 * 4);
  if ((uint16_t)(last >> 16) != 0) { DPORT_INTERRUPT_RESTORE(); return false; }
  ((uint32_t*)ptr)[7] = last;
  for (int i = 0; i < 7; i++) ((uint32_t*)ptr)[i] = DPORT_SEQUENCE_REG_READ(SHA_H_BASE + i * 4);
  DPORT_INTERRUPT_RESTORE();
  return true;
}

static inline void write_digest(const void* digest_state) {
  const uint32_t* w = (const uint32_t*)digest_state;
  uint32_t* reg = (uint32_t*)(SHA_H_BASE);
  for (int i = 0; i < 8; i++) REG_WRITE(&reg[i], w[i]);
}

static inline void wait_idle() { while (REG_READ(SHA_BUSY_REG)) {} }

static void hashTask(void*) {
  std::shared_ptr<JobRequest> job;
  std::shared_ptr<JobResult> result;
  uint8_t hash[32], digest_mid[32], sha_buffer[64];

  for (;;) {
    {
      std::lock_guard<std::mutex> lock(s_job_mutex);
      if (result) {
        if (s_job_result_list.size() < 16) s_job_result_list.push_back(result);
        result.reset();
      }
      if (!s_job_request_list_hw.empty()) {
        job = s_job_request_list_hw.front();
        s_job_request_list_hw.pop_front();
      } else {
        job.reset();
      }
    }
    if (!job) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }

    result = std::make_shared<JobResult>();
    result->id = job->id;
    result->nonce = 0xFFFFFFFF;
    result->nonce_count = job->nonce_count;
    result->difficulty = job->difficulty;
    const uint8_t job_in_work = job->id & 0xFF;
    memcpy(digest_mid, job->midstate, sizeof(digest_mid));
    memcpy(sha_buffer, job->sha_buffer + 64, sizeof(sha_buffer));

    esp_sha_acquire_hardware();
    REG_WRITE(SHA_MODE_REG, SHA2_256);
    const uint32_t nend = job->nonce_start + job->nonce_count;
    for (uint32_t n = job->nonce_start; n < nend; ++n) {
      write_digest(digest_mid);
      fill_text_block(sha_buffer, n);
      REG_WRITE(SHA_CONTINUE_REG, 1);
      sha_ll_load(SHA2_256);
      wait_idle();
      fill_text_block_inter();
      REG_WRITE(SHA_START_REG, 1);
      sha_ll_load(SHA2_256);
      wait_idle();
      if (read_digest_if(hash)) {
        const double diff_hash = diff_from_target(hash);
        if (diff_hash > bestDiff && isSha256Valid(hash)) bestDiff = diff_hash;   // badge: la mejor del badge, la acepte el pool o no
        if (diff_hash > result->difficulty && isSha256Valid(hash)) {
          result->difficulty = diff_hash;
          result->nonce = n;
          memcpy(result->hash, hash, sizeof(hash));
        }
      }
      if ((uint8_t)(n & 0xFF) == 0 && s_working_current_job_id != job_in_work) {
        result->nonce_count = n - job->nonce_start + 1;
        break;
      }
    }
    esp_sha_release_hardware();
    vTaskDelay(1);   // badge: dejar respirar al resto de tareas del núcleo 0 (WiFi, Nostr...)
  }
}

// ---------------------------------------------------------------- lo que usa el badge

void start(const Config& config, bool (*online)(), void (*onSocket)(bool)) {
  cfg = config;
  isOnline = online;
  socketOpen = onSocket;
  wantRunning = true;
  // badge: mientras mina, la tarea de reposo del núcleo 0 casi no corre y el watchdog saltaría.
  // Como en el NerdMiner original, se la quita del watchdog; al parar el minero se vuelve a poner.
  disableCore0WDT();
  if (!tasksStarted) {
    xTaskCreatePinnedToCore(stratumTask, "stratum", 12288, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(hashTask, "sha", 6144, nullptr, 1, nullptr, 0);
    tasksStarted = true;
  }
}

void stop() {
  wantRunning = false;
  enableCore0WDT();
}

Stats stats() {
  static uint32_t lastHashes = 0, lastAt = 0;
  static float khs = 0;
  const uint32_t now = millis();
  if (now - lastAt >= 2000) {                  // kH/s con lo hecho en los últimos ~2 s
    const uint32_t h = hashes;
    khs = lastAt ? (h - lastHashes) / (float)(now - lastAt) : 0;   // hashes/ms = kH/s
    lastHashes = h;
    lastAt = now;
  }
  Stats s;
  s.running = wantRunning;
  s.connected = poolReady;
  s.khs = wantRunning ? khs : 0;
  s.shares = shares;
  s.blocks = valids;
  s.bestDiff = bestDiff;
  s.templates = templates;
  s.lastShareAt = lastShareAt;
  s.accepted = accepted;
  s.rejected = rejected;
  s.poolDiff = poolDiff;
  s.minedMs = minedMs;
  return s;
}

}  // namespace nerdminer
