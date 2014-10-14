// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_SNAPSHOT_EXCEPTION_SNAPSHOT_MAC_H_
#define CRASHPAD_SNAPSHOT_EXCEPTION_SNAPSHOT_MAC_H_

#include <mach/mach.h>
#include <stdint.h>

#include <vector>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "snapshot/cpu_context.h"
#include "snapshot/exception_snapshot.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

class ProcessReader;

namespace internal {

//! \brief An ExceptionSnapshot of an exception sustained by a running (or
//!     crashed) process on a Mac OS X system.
class ExceptionSnapshotMac final : public ExceptionSnapshot {
 public:
  ExceptionSnapshotMac();
  ~ExceptionSnapshotMac();

  //! \brief Initializes the object.
  //!
  //! Other than \a process_reader, the parameters may be passed directly
  //! through from a Mach exception handler.
  //!
  //! \param[in] process_reader A ProcessReader for the task that sustained the
  //!     exception.
  //!
  //! \return `true` if the snapshot could be created, `false` otherwise with
  //!     an appropriate message logged.
  bool Initialize(ProcessReader* process_reader,
                  thread_t exception_thread,
                  exception_type_t exception,
                  const mach_exception_data_type_t* code,
                  mach_msg_type_number_t code_count,
                  thread_state_flavor_t flavor,
                  const natural_t* state,
                  mach_msg_type_number_t state_count);

  // ExceptionSnapshot:

  virtual const CPUContext* Context() const override;
  virtual uint64_t ThreadID() const override;
  virtual uint32_t Exception() const override;
  virtual uint32_t ExceptionInfo() const override;
  virtual uint64_t ExceptionAddress() const override;
  virtual const std::vector<uint64_t>& Codes() const override;

 private:
#if defined(ARCH_CPU_X86_FAMILY)
  union {
    CPUContextX86 x86;
    CPUContextX86_64 x86_64;
  } context_union_;
#endif
  CPUContext context_;
  std::vector<uint64_t> codes_;
  uint64_t thread_id_;
  uint64_t exception_address_;
  exception_type_t exception_;
  uint32_t exception_code_0_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionSnapshotMac);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_EXCEPTION_SNAPSHOT_MAC_H_