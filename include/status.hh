#pragma once

enum class Status : std::int32_t {
  OK,
  WARN_ALREADY_EXISTS,
  WARN_CONCURRENT_DELETE,
  WARN_NOT_FOUND,
};
