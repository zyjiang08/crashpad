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

#ifndef CRASHPAD_UTIL_MAC_MACH_O_IMAGE_SEGMENT_READER_H_
#define CRASHPAD_UTIL_MAC_MACH_O_IMAGE_SEGMENT_READER_H_

#include <mach/mach.h>
#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "util/mac/process_types.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

class ProcessReader;

//! \brief A reader for LC_SEGMENT or LC_SEGMENT_64 load commands in Mach-O
//!     images mapped into another process.
//!
//! This class is capable of reading both LC_SEGMENT and LC_SEGMENT_64 based on
//! the bitness of the remote process.
//!
//! A MachOImageSegmentReader will normally be instantiated by a
//! MachOImageReader.
class MachOImageSegmentReader {
 public:
  MachOImageSegmentReader();
  ~MachOImageSegmentReader();

  //! \brief Reads the segment load command from another process.
  //!
  //! This method must only be called once on an object. This method must be
  //! called successfully before any other method in this class may be called.
  //!
  //! \param[in] process_reader The reader for the remote process.
  //! \param[in] load_command_address The address, in the remote process’
  //!     address space, where the LC_SEGMENT or LC_SEGMENT_64 load command
  //!     to be read is located. This address is determined by a Mach-O image
  //!     reader, such as MachOImageReader, as it walks Mach-O load commands.
  //! \param[in] load_command_info A string to be used in logged messages. This
  //!     string is for diagnostic purposes only, and may be empty.
  //!
  //! \return `true` if the load command was read successfully. `false`
  //!     otherwise, with an appropriate message logged.
  bool Initialize(ProcessReader* process_reader,
                  mach_vm_address_t load_command_address,
                  const std::string& load_command_info);

  //! \brief Returns the segment’s name.
  //!
  //! The segment’s name is taken from the load command’s `segname` field.
  //! Common segment names are `"__TEXT"`, `"__DATA"`, and `"__LINKEDIT"`.
  //! Symbolic constants for these common names are defined in
  //! `<mach-o/loader.h>`.
  std::string Name() const;

  //! \brief The segment’s preferred load address.
  //!
  //! \return The segment’s preferred load address as stored in the Mach-O file.
  //!
  //! \note This value is not adjusted for any “slide” that may have occurred
  //!     when the image was loaded.
  //!
  //! \sa MachOImageReader::GetSegmentByName()
  mach_vm_address_t vmaddr() const { return segment_command_.vmaddr; }

  //! \brief Returns the segment’s size as mapped into memory.
  mach_vm_size_t vmsize() const { return segment_command_.vmsize; }

  //! \brief Returns the file offset of the mapped segment in the file from
  //!     which it was mapped.
  //!
  //! The file offset is the difference between the beginning of the
  //! `mach_header` or `mach_header_64` and the beginning of the segment’s
  //! mapped region. For segments that are not mapped from a file (such as
  //! `"__PAGEZERO"` segments), this will be `0`.
  mach_vm_size_t fileoff() const { return segment_command_.fileoff; }

  //! \brief Returns the number of sections in the segment.
  //!
  //! This will return `0` for a segment without any sections, typical for
  //! `"__PAGEZERO"` and `"__LINKEDIT"` segments.
  //!
  //! Although the Mach-O file format uses a `uint32_t` for this field, there is
  //! an overall limit of 255 sections in an entire Mach-O image file (not just
  //! in a single segment) imposed by the symbol table format. Symbols will not
  //! be able to reference anything in a section beyond the first 255 in a
  //! Mach-O image file.
  uint32_t nsects() const { return segment_command_.nsects; }

  //! \brief Obtain section information by section name.
  //!
  //! \param[in] section_name The name of the section to search for, without the
  //!     leading segment name. For example, use `"__text"`, not
  //!     `"__TEXT,__text"` or `"__TEXT.__text"`.
  //!
  //! \return A pointer to the section information if it was found, or `NULL` if
  //!     it was not found.
  //!
  //! \note The process_types::section::addr field gives the section’s preferred
  //!     load address as stored in the Mach-O image file, and is not adjusted
  //!     for any “slide” that may have occurred when the image was loaded.
  //!
  //! \sa MachOImageReader::GetSectionByName()
  const process_types::section* GetSectionByName(
      const std::string& section_name) const;

  //! \brief Obtain section information by section index.
  //!
  //! \param[in] index The index of the section to return, in the order that it
  //!     appears in the segment load command. Unlike
  //!     MachOImageReader::GetSectionAtIndex(), this is a 0-based index. This
  //!     parameter must be in the range of valid indices aas reported by
  //!     nsects().
  //!
  //! \return A pointer to the section information. If \a index is out of range,
  //!     execution is aborted.
  //!
  //! \note The process_types::section::addr field gives the section’s preferred
  //!     load address as stored in the Mach-O image file, and is not adjusted
  //!     for any “slide” that may have occurred when the image was loaded.
  //! \note Unlike MachOImageReader::GetSectionAtIndex(), this method does not
  //!     accept out-of-range values for \a index, and aborts execution instead
  //!     of returning `NULL` upon encountering an out-of-range value. This is
  //!     because this method is expected to be used in a loop that can be
  //!     limited to nsects() iterations, so an out-of-range error can be
  //!     treated more harshly as a logic error, as opposed to a data error.
  //!
  //! \sa MachOImageReader::GetSectionAtIndex()
  const process_types::section* GetSectionAtIndex(size_t index) const;

  //! Returns whether the segment slides.
  //!
  //! Most segments slide, but the __PAGEZERO segment does not, it grows
  //! instead. This method identifies non-sliding segments in the same way that
  //! the kernel does.
  bool SegmentSlides() const;

  //! \brief Returns a segment name string.
  //!
  //! Segment names may be 16 characters long, and are not necessarily
  //! `NUL`-terminated. This function will return a segment name based on up to
  //! the first 16 characters found at \a segment_name_c.
  static std::string SegmentNameString(const char* segment_name_c);

  //! \brief Returns a section name string.
  //!
  //! Section names may be 16 characters long, and are not necessarily
  //! `NUL`-terminated. This function will return a section name based on up to
  //! the first 16 characters found at \a section_name_c.
  static std::string SectionNameString(const char* section_name_c);

  //! \brief Returns a segment and section name string.
  //!
  //! A segment and section name string is composed of a segment name string
  //! (see SegmentNameString()) and a section name string (see
  //! SectionNameString()) separated by a comma. An example is
  //! `"__TEXT,__text"`.
  static std::string SegmentAndSectionNameString(const char* segment_name_c,
                                                 const char* section_name_c);

 private:
  //! \brief The internal implementation of Name().
  //!
  //! This is identical to Name() but does not perform the
  //! InitializationStateDcheck check. It may be called during initialization
  //! provided that the caller only does so after segment_command_ has been
  //! read successfully.
  std::string NameInternal() const;

  // The segment command data read from the remote process.
  process_types::segment_command segment_command_;

  // Section structures read from the remote process in the order that they are
  // given in the remote process.
  std::vector<process_types::section> sections_;

  // Maps section names to indices into the sections_ vector.
  std::map<std::string, size_t> section_map_;

  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(MachOImageSegmentReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MAC_MACH_O_IMAGE_SEGMENT_READER_H_