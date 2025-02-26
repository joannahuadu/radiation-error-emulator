#ifndef SYS_TOOLS_H
#define SYS_TOOLS_H

#include <cstdint>

/**
 * Get the maximum number of bits of the physical address.
 * sudo permission is required.
 * @return The maximum number of bits of the physical address.
 * @throw std::runtime_error if failed to open memory info file.
 * @throw std::runtime_error if failed to read memory info.
 * @throw std::runtime_error if invalid memory block format.
 */
uint64_t getMaxPhysicalAddressBits();

/**
 * Get the page size.
 * @return The page size.
 */
uint64_t getPageSize();

#endif // SYS_TOOLS_H
