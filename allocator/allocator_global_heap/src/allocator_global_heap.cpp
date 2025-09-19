#include <not_implemented.h>
#include "../include/allocator_global_heap.h"

allocator_global_heap::allocator_global_heap(
    logger *logger): _logger(logger)
{
    trace_with_guard("START: " + get_typename() + ": constructor");
    trace_with_guard("END: " + get_typename() + ": constructor");
}

[[nodiscard]] void *allocator_global_heap::do_allocate_sm(
    size_t size)
{
    debug_with_guard("START: " + get_typename() + ": allocate");

    if (size == 0) {
        warning_with_guard("WARNING: " + get_typename() + ": zero-size allocate");
    }
    void *ptr;
    try {
        ptr = ::operator new(size);
    }
    catch (const std::bad_alloc& ex) {
        error_with_guard("ERROR: " + get_typename() + ": failed to allocate " + std::to_string(size) + " bytes: " + ex.what());
        throw;
    }
    catch (...) {
        error_with_guard("ERROR: " + get_typename() + ": unknown error during allocation");
        throw std::bad_alloc();
    }

    if (ptr == nullptr) {
        error_with_guard("ERROR: " + get_typename() + ": allocation returned nullptr");
        throw std::bad_alloc();
    }

    debug_with_guard("END: " + get_typename() + ": allocate");



    return ptr;
}

void allocator_global_heap::do_deallocate_sm(
    void *at)
{
    debug_with_guard("START: " + get_typename() + ": deallocate");
    ::operator delete(at);
    debug_with_guard("END: " + get_typename() + ": deallocate");
}

inline logger *allocator_global_heap::get_logger() const
{
    return _logger;
}

inline std::string allocator_global_heap::get_typename() const
{
    return "allocator_global_heap";
}

allocator_global_heap::~allocator_global_heap()
{
    trace_with_guard("START: " + get_typename() + ": destructor");
    trace_with_guard("END: " + get_typename() + ": destructor");
}

allocator_global_heap::allocator_global_heap(const allocator_global_heap &other)
{
    trace_with_guard("START: " + get_typename() + ": copy constructor");

    if (this != &other) {
        _logger = other._logger;
    } else {
        warning_with_guard("WARNING: " + get_typename() + ": self-copy attempt in constructor");
    }

    trace_with_guard("END: " + get_typename() + ": copy constructor");
}

allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
{
    trace_with_guard("START: " + get_typename() + ": copy operator=");

    if (this != &other) {
        _logger = other._logger;
    } else {
        trace_with_guard("NOTE: " + get_typename() + ": self-assignment detected, skipping");
    }

    trace_with_guard("END: " + get_typename() + ": copy operator=");

    return *this;
}

bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return dynamic_cast<const allocator_global_heap*>(&other) != nullptr;
}

allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept
{
    trace_with_guard("START: " + get_typename() + ": move constructor");
    _logger = other._logger;
    other._logger = nullptr;
    trace_with_guard("END: " + get_typename() + ": move constructor");
}

allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept
{
    trace_with_guard("START: " + get_typename() + ": move operator");

    if (this != &other) {
        std::swap(_logger, other._logger);
    }

    trace_with_guard("END: " + get_typename() + ": move operator");

    return *this;
}