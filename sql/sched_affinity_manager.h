/*****************************************************************************
Copyright (c) 2020, Huawei Technologies Co., Ltd. All Rights Reserved.
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.
This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.
*****************************************************************************/

#ifndef SCHED_AFFINITY_MANAGER_H
#define SCHED_AFFINITY_MANAGER_H
#include "my_config.h"
#ifdef HAVE_LIBNUMA
#include <numa.h>
#endif

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

#include "mysql/psi/mysql_mutex.h"

namespace sched_affinity {

enum class Thread_type {
  FOREGROUND,
  LOG_WRITER,
  LOG_FLUSHER,
  LOG_WRITE_NOTIFIER,
  LOG_FLUSH_NOTIFIER,
  LOG_CLOSER,
  LOG_CHECKPOINTER,
  PURGE_COORDINATOR,
  UNDEFINED
};

pid_t gettid();

class Sched_affinity_manager {
 public:
  static Sched_affinity_manager *create_instance(
      const std::map<Thread_type, const char *> &, bool numa_aware);
  static Sched_affinity_manager *get_instance();
  static void free_instance();
  virtual bool register_thread(const Thread_type thread_type,
                               const pid_t pid) = 0;
  virtual bool unregister_thread(const Thread_type thread_type,
                                 const pid_t pid) = 0;
  virtual bool rebalance_group(const char *, const Thread_type thread_type,
                               const bool numa_aware) = 0;
  virtual std::string take_group_snapshot() = 0;
  virtual int get_total_node_number() = 0;
  virtual int get_cpu_number_per_node() = 0;
  virtual bool check_cpu_string(const std::string &cpu_string) = 0;

 protected:
  virtual bool init(const std::map<Thread_type, const char *> &,
                    bool numa_aware) = 0;
  virtual ~Sched_affinity_manager() {}
};

class Sched_affinity_manager_dummy : public Sched_affinity_manager {
 public:
  Sched_affinity_manager_dummy(const Sched_affinity_manager_dummy &) = delete;
  Sched_affinity_manager_dummy &operator=(
      const Sched_affinity_manager_dummy &) = delete;
  Sched_affinity_manager_dummy(const Sched_affinity_manager_dummy &&) = delete;
  Sched_affinity_manager_dummy &operator=(
      const Sched_affinity_manager_dummy &&) = delete;
  bool register_thread(const Thread_type, const pid_t) override { return true; }
  bool unregister_thread(const Thread_type, const pid_t) override {
    return true;
  }
  bool rebalance_group(const char *, const Thread_type, const bool) { return true; }
  std::string take_group_snapshot() override;
  int get_total_node_number() override { return -1; }
  int get_cpu_number_per_node() override { return -1; }
  bool check_cpu_string(const std::string &) override { return true; }

 private:
  Sched_affinity_manager_dummy() : Sched_affinity_manager(){};
  ~Sched_affinity_manager_dummy(){};
  bool init(const std::map<Thread_type, const char *> &, bool) override {
    return true;
  }
  friend class Sched_affinity_manager;
};

#ifdef HAVE_LIBNUMA

struct Sched_affinity_group {
  bitmask *avail_cpu_mask;
  int avail_cpu_num;
  int assigned_thread_num;
};

class Sched_affinity_manager_numa : public Sched_affinity_manager {
 public:
  Sched_affinity_manager_numa(const Sched_affinity_manager_numa &) = delete;
  Sched_affinity_manager_numa &operator=(const Sched_affinity_manager_numa &) =
      delete;
  Sched_affinity_manager_numa(const Sched_affinity_manager_numa &&) = delete;
  Sched_affinity_manager_numa &operator=(const Sched_affinity_manager_numa &&) =
      delete;

  bool register_thread(const Thread_type thread_type, const pid_t pid) override;
  bool unregister_thread(const Thread_type thread_type,
                         const pid_t pid) override;
  bool rebalance_group(const char *, const Thread_type thread_type,
                       const bool numa_aware) override;
  std::string take_group_snapshot() override;
  int get_total_node_number() override;
  int get_cpu_number_per_node() override;
  bool check_cpu_string(const std::string &cpu_string) override;

 private:
  Sched_affinity_manager_numa();
  ~Sched_affinity_manager_numa();
  bool init(const std::map<Thread_type, const char *> &, bool) override;
  bool init_sched_affinity_info(const std::string &cpu_string,
                                       bitmask *&group_bitmask);
  bool init_sched_affinity_group(const bitmask *group_bitmask,
                                        const bool numa_aware,
                                        std::vector<Sched_affinity_group> &sched_affinity_group);
  bool is_thread_sched_enabled(const Thread_type thread_type);
  bool bind_to_group(const pid_t pid);
  bool unbind_from_group(const pid_t pid);
  Thread_type get_thread_type_by_pid(const pid_t pid);
  bool are_bitmasks_overlapped(bitmask *, bitmask *);
  static std::pair<std::string, bool> normalize_cpu_string(
      const std::string &cpu_string);
  friend class Sched_affinity_manager;
  friend class SchedAffinityManagerTest;

 private:
  int m_total_cpu_num;
  int m_total_node_num;
  int m_cpu_num_per_node;
  std::map<Thread_type, std::vector<Sched_affinity_group>>
      m_sched_affinity_group;
  std::map<Thread_type, bitmask *> m_thread_bitmask;
  std::map<Thread_type, std::set<pid_t>> m_thread_pid;
  std::map<pid_t, int> m_pid_group_id;
  mysql_mutex_t m_mutex;
};
#endif /* HAVE_LIBNUMA */
}  // namespace sched_affinity
#endif /* SCHED_AFFINITY_MANAGER_H */
