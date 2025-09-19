#include "../include/allocator_boundary_tags.h"

allocator_boundary_tags::~allocator_boundary_tags() {
    logger* allocator_logger = get_logger();
    allocator_logger->debug("Начало удаления аллокатора");

    if (!_trusted_memory) {
        allocator_logger->error("Нет доверенной памяти для освобождения");
        return;
    }

    get_mutex().~mutex();

    try {
        memory_resource* parent_allocator = get_parent_resource();
        size_t total_memory_size = allocator_metadata_size + reinterpret_cast<size_t>(static_cast<char*>(_trusted_memory) +
                                  sizeof(logger*) + sizeof(memory_resource*) +
                                  sizeof(allocator_with_fit_mode::fit_mode));

        if (parent_allocator != nullptr) {
            parent_allocator->deallocate(_trusted_memory, total_memory_size);
            allocator_logger->trace("Память освобождена через родительский аллокатор");
        } else {
            ::operator delete(_trusted_memory);
            allocator_logger->trace("Память освобождена через глобальный оператор delete");
        }

        _trusted_memory = nullptr;
    } catch (const std::exception& exception) {
        allocator_logger->error("Ошибка при удалении аллокатора: " + std::string(exception.what()));
        _trusted_memory = nullptr;
    }

    allocator_logger->trace("Завершение удаления аллокатора");
}

allocator_boundary_tags::allocator_boundary_tags(allocator_boundary_tags&& other) noexcept : _trusted_memory(other._trusted_memory) {
    trace_with_guard("Начало конструктора перемещения");

    std::lock_guard<std::mutex> lock(other.get_mutex());
    other._trusted_memory = nullptr;

    get_logger()->debug("Ресурсы перемещены");
    trace_with_guard("Завершение конструктора перемещения");
}

allocator_boundary_tags& allocator_boundary_tags::operator=(allocator_boundary_tags&& other) noexcept {
    trace_with_guard("Начало оператора перемещения");
    logger* allocator_logger = get_logger();

    if (this != &other) {
        std::unique_lock<std::mutex> this_lock(get_mutex(), std::defer_lock);
        std::unique_lock<std::mutex> other_lock(other.get_mutex(), std::defer_lock);
        std::lock(this_lock, other_lock);

        if (_trusted_memory != nullptr) {
            try {
                size_t total_memory_size = *reinterpret_cast<size_t*>(reinterpret_cast<unsigned char*>(_trusted_memory) +
                                    sizeof(logger*) + sizeof(memory_resource*) +
                                    sizeof(allocator_with_fit_mode::fit_mode));

                memory_resource* parent_allocator = get_parent_resource();
                if (parent_allocator != nullptr) {
                    parent_allocator->deallocate(_trusted_memory, total_memory_size);
                } else {
                    ::operator delete(_trusted_memory);
                }
            } catch (const std::exception& exception) {
                allocator_logger->error("Ошибка в операторе перемещения: " + std::string(exception.what()));
            }
        }

        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
        allocator_logger->debug("Ресурсы перемещены");
    }

    trace_with_guard("Завершение оператора перемещения");
    return *this;
}

allocator_boundary_tags::allocator_boundary_tags(size_t space_size, std::pmr::memory_resource *parent_allocator, logger *logger, allocator_with_fit_mode::fit_mode allocate_fit_mode) {
    logger->debug("НАЧАЛО КОНСТРУКТОРА.");
    if (space_size == 0) {
        logger->error("Размер должен быть > 0.");
        throw std::invalid_argument("Размер должен быть > 0");
    }

    try {
        parent_allocator = parent_allocator ? parent_allocator : std::pmr::get_default_resource();
        size_t total_size = allocator_metadata_size + space_size;
        logger->debug("ТРЕБУЕТСЯ " + std::to_string(total_size) + " байт.");
        logger->information("ДОСТУПНО" + std::to_string(space_size) + " байт.");

        _trusted_memory = parent_allocator->allocate(total_size);
        auto *memory = reinterpret_cast<unsigned char *>(_trusted_memory);
        *reinterpret_cast<class logger **>(memory) = logger;
        memory += sizeof(class logger *);

        *reinterpret_cast<std::pmr::memory_resource **>(memory) = parent_allocator;
        memory += sizeof(memory_resource *);

        *reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory) = allocate_fit_mode;
        memory += sizeof(allocator_with_fit_mode::fit_mode);

        *reinterpret_cast<size_t *>(memory) = space_size;
        memory += sizeof(size_t);

        new (reinterpret_cast<std::mutex *>(memory)) std::mutex();
        memory += sizeof(std::mutex);

        *reinterpret_cast<void **>(memory) = nullptr;
    } catch (const std::exception &e) {
        logger->error("ОШИБКА В коНСТРУкторе");
        throw std::iostream ::failure("ОШИБКА В коНСТРУкторе");
    }

    logger->debug("ЗАВЕРШЕНИЕ РАБОТЫ КОНСТРУКТОРА");
}

[[nodiscard]] void* allocator_boundary_tags::do_allocate_sm(size_t size) {
    logger* allocator_logger = get_logger();
    allocator_logger->debug("Начало выделения памяти");

    const size_t total_required_size = size + occupied_block_metadata_size;
    size_t allocator_total_size = *reinterpret_cast<size_t*>(reinterpret_cast<char*>(_trusted_memory) +
                                 sizeof(logger*) + sizeof(memory_resource*) +
                                 sizeof(allocator_with_fit_mode::fit_mode));

    if (total_required_size > allocator_total_size) {
        allocator_logger->error("Недостаточно памяти для выделения");
        throw std::bad_alloc();
    }

    void* allocated_block = nullptr;
    allocator_with_fit_mode::fit_mode current_fit_mode = get_fit_mode();

    if (current_fit_mode == fit_mode::first_fit) {
        allocated_block = allocate_first_fit(size);
    } else if (current_fit_mode == fit_mode::the_best_fit) {
        allocated_block = allocate_best_fit(size);
    } else if (current_fit_mode == fit_mode::the_worst_fit) {
        allocated_block = allocate_worst_fit(size);
    } else {
        allocator_logger->error("Неизвестный режим выделения памяти");
        throw std::invalid_argument("Неизвестный режим выделения памяти");
    }

    if (allocated_block == nullptr) {
        allocator_logger->error("Ошибка выделения памяти для размера " + std::to_string(size));
        throw std::bad_alloc();
    }

    allocator_logger->debug("Завершение выделения памяти");
    return allocated_block;
}

void allocator_boundary_tags::do_deallocate_sm(void* block_ptr) {
    logger* allocator_logger = get_logger();
    allocator_logger->debug("Начало освобождения памяти");
    std::lock_guard<std::mutex> guard(get_mutex());

    if (block_ptr == nullptr) {
        allocator_logger->warning("Попытка освободить nullptr");
        return;
    }


    void* block_allocator = *reinterpret_cast<void**>(reinterpret_cast<char*>(block_ptr) + sizeof(size_t) + 2 * sizeof(void*));
    if (block_allocator != _trusted_memory) {
        allocator_logger->error("Попытка освободить блок, не принадлежащий этому аллокатору");
        throw std::logic_error("Блок не был выделен этим аллокатором");
    }

    char* heap_start = reinterpret_cast<char*>(_trusted_memory) + allocator_metadata_size;
    void* next_block = *reinterpret_cast<void**>(reinterpret_cast<char*>(block_ptr) + sizeof(size_t));
    void* prev_block = *reinterpret_cast<void**>(reinterpret_cast<char*>(block_ptr) + sizeof(size_t) + sizeof(void*));

    if (prev_block != nullptr) {
        *reinterpret_cast<void**>(reinterpret_cast<char*>(prev_block) + sizeof(size_t)) = next_block;
    } else {
        void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));
        *first_block_ptr = next_block;
    }

    if (next_block != nullptr) {
        *reinterpret_cast<void**>(reinterpret_cast<char*>(next_block) + sizeof(size_t) + sizeof(void*)) = prev_block;
    }

    allocator_logger->debug("Завершение освобождения памяти");
}

allocator_with_fit_mode::fit_mode allocator_boundary_tags::get_fit_mode() const {
    unsigned char* memory_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    memory_ptr += sizeof(logger*) + sizeof(memory_resource*);
    return *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(memory_ptr);
}

inline void allocator_boundary_tags::set_fit_mode(allocator_with_fit_mode::fit_mode mode) {
    unsigned char* memory_ptr = reinterpret_cast<unsigned char*>(_trusted_memory);
    allocator_with_fit_mode::fit_mode* mode_ptr = reinterpret_cast<allocator_with_fit_mode::fit_mode*>(
        memory_ptr + sizeof(logger*) + sizeof(memory_resource*));
    *mode_ptr = mode;
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const {
    logger* allocator_logger = get_logger();
    allocator_logger->trace("Начало получения информации о блоках");

    std::vector<allocator_test_utils::block_info> blocks_info;

    if (_trusted_memory == nullptr) {
        allocator_logger->error("Нет доверенной памяти");
        return blocks_info;
    }

    try {
        char* heap_start = reinterpret_cast<char*>(_trusted_memory) + allocator_metadata_size;
        size_t heap_size = *reinterpret_cast<size_t*>(reinterpret_cast<char*>(_trusted_memory) +
                         sizeof(logger*) + sizeof(memory_resource*) +
                         sizeof(allocator_with_fit_mode::fit_mode));
        char* heap_end = heap_start + heap_size;

        void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));
        char* current_block = reinterpret_cast<char*>(*first_block_ptr);

        while (current_block != nullptr && current_block < heap_end) {
            size_t block_size = *reinterpret_cast<size_t*>(current_block);
            void* next_block = *reinterpret_cast<void**>(current_block + sizeof(size_t));
            bool is_block_occupied = true;

            blocks_info.push_back({
                .block_size = block_size + occupied_block_metadata_size,
                .is_block_occupied = is_block_occupied
            });

            current_block = reinterpret_cast<char*>(next_block);
        }

        current_block = reinterpret_cast<char*>(*first_block_ptr);
        char* prev_block_end = heap_start;

        while (current_block != nullptr && current_block < heap_end) {
            if (current_block > prev_block_end) {
                size_t hole_size = current_block - prev_block_end;
                blocks_info.push_back({
                    .block_size = hole_size,
                    .is_block_occupied = false
                });
            }

            size_t block_size = *reinterpret_cast<size_t*>(current_block);
            prev_block_end = current_block + occupied_block_metadata_size + block_size;
            void* next_block = *reinterpret_cast<void**>(current_block + sizeof(size_t));
            current_block = reinterpret_cast<char*>(next_block);
        }

        if (prev_block_end < heap_end) {
            size_t hole_size = heap_end - prev_block_end;
            blocks_info.push_back({
                .block_size = hole_size,
                .is_block_occupied = false
            });
        }
    } catch (...) {
        allocator_logger->error("Ошибка при итерации по блокам");
        throw;
    }

    return blocks_info;
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const {
    logger* allocator_logger = get_logger();
    allocator_logger->trace("Начало получения информации о блоках");

    std::lock_guard<std::mutex> guard(get_mutex());
    auto result = get_blocks_info_inner();

    allocator_logger->information("Получено " + std::to_string(result.size()) + " блоков");
    allocator_logger->trace("Завершение получения информации о блоках");

    return result;
}

inline logger* allocator_boundary_tags::get_logger() const {
    if (_trusted_memory == nullptr) {
        return nullptr;
    }

    return *reinterpret_cast<logger**>(_trusted_memory);
}

inline std::string allocator_boundary_tags::get_typename() const noexcept {
    return "allocator_boundary_tags";
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept {
    return {reinterpret_cast<char*>(_trusted_memory) + allocator_metadata_size};
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept {
    size_t total_size = *reinterpret_cast<size_t*>(reinterpret_cast<char*>(_trusted_memory) +
                       sizeof(logger*) + sizeof(memory_resource*) +
                       sizeof(allocator_with_fit_mode::fit_mode));

    return {reinterpret_cast<char*>(_trusted_memory) + total_size};
}

bool allocator_boundary_tags::do_is_equal(const memory_resource& other) const noexcept {
    if (this == &other) {
        return true;
    }

    const allocator_boundary_tags* derived = dynamic_cast<const allocator_boundary_tags*>(&other);

    if (derived == nullptr) {
        return false;
    }

    return this->_trusted_memory == derived->_trusted_memory;
}

bool allocator_boundary_tags::boundary_iterator::operator==(const boundary_iterator& other) const noexcept {
    return _occupied_ptr == other._occupied_ptr;
}

bool allocator_boundary_tags::boundary_iterator::operator!=(const boundary_iterator& other) const noexcept {
    return !(*this == other);
}

allocator_boundary_tags::boundary_iterator& allocator_boundary_tags::boundary_iterator::operator++() & noexcept {
    _occupied_ptr = reinterpret_cast<void*>(reinterpret_cast<unsigned char*>(_occupied_ptr) + sizeof(size_t));
    _occupied = true;
    return *this;
}

allocator_boundary_tags::boundary_iterator& allocator_boundary_tags::boundary_iterator::operator--() & noexcept {
    _occupied_ptr = reinterpret_cast<void*>(reinterpret_cast<unsigned char*>(_occupied_ptr) + sizeof(size_t) + sizeof(void*));
    _occupied = true;
    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int) {
    boundary_iterator tmp = *this;
    ++(*this);
    return tmp;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int) {
    boundary_iterator tmp = *this;
    --(*this);
    return tmp;
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept {
    return reinterpret_cast<size_t>(_occupied_ptr);
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept {
    return _occupied;
}

void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept {
    return reinterpret_cast<void*>(reinterpret_cast<unsigned char*>(_occupied_ptr) + sizeof(size_t));
}

allocator_boundary_tags::boundary_iterator::boundary_iterator() :
    _occupied_ptr(nullptr), _occupied(false), _trusted_memory(nullptr) {}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void* trusted) :
    _trusted_memory(trusted) {
    _occupied_ptr = trusted;
    size_t tag = reinterpret_cast<size_t>(_occupied_ptr);
    _occupied = true;
}

void* allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept {
    return _occupied_ptr;
}

std::pmr::memory_resource* allocator_boundary_tags::get_parent_resource() const noexcept {
    if (_trusted_memory == nullptr) {
        return nullptr;
    }
    return *reinterpret_cast<memory_resource**>(reinterpret_cast<unsigned char*>(_trusted_memory) + sizeof(logger*));
}

inline std::mutex& allocator_boundary_tags::get_mutex() const {
    if (_trusted_memory == nullptr) {
        throw std::logic_error("Мьютекс не существует");
    }

    unsigned char* ptr = reinterpret_cast<unsigned char*>(_trusted_memory) +
                        sizeof(logger*) + sizeof(memory_resource*) +
                        sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t);
    return *reinterpret_cast<std::mutex*>(ptr);
}

void* allocator_boundary_tags::allocate_first_fit(size_t size) {
    std::lock_guard<std::mutex> guard(get_mutex());
    const size_t total_required_size = size + occupied_block_metadata_size;

    size_t allocator_total_size = *reinterpret_cast<size_t*>(static_cast<char*>(_trusted_memory) +
                                sizeof(logger*) + sizeof(memory_resource*) +
                                sizeof(allocator_with_fit_mode::fit_mode));

    char* heap_start = static_cast<char*>(_trusted_memory) + allocator_metadata_size;
    char* heap_end = heap_start + allocator_total_size;
    void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));

    if (*first_block_ptr == nullptr) {
        if (heap_end - heap_start >= total_required_size) {
            return allocate_new_block(heap_start, size, first_block_ptr, heap_end - heap_start);
        }
        return nullptr;
    }

    size_t front_hole_size = static_cast<char*>(*first_block_ptr) - heap_start;
    if (front_hole_size >= total_required_size) {
        return allocate_in_hole(heap_start, size, first_block_ptr, nullptr, *first_block_ptr, front_hole_size);
    }

    void* current_block = *first_block_ptr;
    while (current_block != nullptr) {
        size_t current_block_size = *reinterpret_cast<size_t*>(current_block);
        void* next_block = *reinterpret_cast<void**>(static_cast<char*>(current_block) + sizeof(size_t));
        char* current_block_end = static_cast<char*>(current_block) + occupied_block_metadata_size + current_block_size;

        if (next_block == nullptr) {
            size_t end_hole_size = heap_end - current_block_end;
            if (end_hole_size >= total_required_size) {
                return allocate_in_hole(current_block_end, size, first_block_ptr, current_block, nullptr, end_hole_size);
            }
            break;
        }

        size_t middle_hole_size = static_cast<char*>(next_block) - current_block_end;
        if (middle_hole_size >= total_required_size) {
            return allocate_in_hole(current_block_end, size, first_block_ptr, current_block, next_block, middle_hole_size);
        }

        current_block = next_block;
    }

    return nullptr;
}

void* allocator_boundary_tags::allocate_best_fit(size_t size) {
    std::lock_guard<std::mutex> guard(get_mutex());
    const size_t total_required_size = size + occupied_block_metadata_size;

    size_t allocator_total_size = *reinterpret_cast<size_t*>(static_cast<char*>(_trusted_memory) +
                                sizeof(logger*) + sizeof(memory_resource*) +
                                sizeof(allocator_with_fit_mode::fit_mode));

    char* heap_start = static_cast<char*>(_trusted_memory) + allocator_metadata_size;
    char* heap_end = heap_start + allocator_total_size;
    void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));

    void* best_block = nullptr;
    void* best_prev_block = nullptr;
    void* best_next_block = nullptr;
    size_t best_size_diff = SIZE_MAX;

    if (*first_block_ptr != nullptr) {
        size_t front_hole_size = static_cast<char*>(*first_block_ptr) - heap_start;
        if (front_hole_size >= total_required_size) {
            best_block = heap_start;
            best_prev_block = nullptr;
            best_next_block = *first_block_ptr;
            best_size_diff = front_hole_size - total_required_size;
        }
    } else {
        if (heap_end - heap_start >= total_required_size) {
            return allocate_new_block(heap_start, size, first_block_ptr, heap_end - heap_start);
        }
        return nullptr;
    }

    void* current_block = *first_block_ptr;
    while (current_block != nullptr) {
        size_t current_block_size = *reinterpret_cast<size_t*>(current_block);
        void* next_block = *reinterpret_cast<void**>(static_cast<char*>(current_block) + sizeof(size_t));
        char* current_block_end = static_cast<char*>(current_block) + occupied_block_metadata_size + current_block_size;

        size_t hole_size = (next_block != nullptr) ? static_cast<char*>(next_block) - current_block_end : heap_end - current_block_end;

        if (hole_size >= total_required_size) {
            size_t size_diff = hole_size - total_required_size;
            if (size_diff < best_size_diff) {
                best_block = current_block_end;
                best_prev_block = current_block;
                best_next_block = next_block;
                best_size_diff = size_diff;

                if (size_diff == 0) {
                    break;
                }
            }
        }

        current_block = next_block;
    }

    if (best_block != nullptr) {
        return allocate_in_hole(reinterpret_cast<char*>(best_block), size, first_block_ptr,
                              best_prev_block, best_next_block, best_size_diff);
    }
    return nullptr;
}

void* allocator_boundary_tags::allocate_worst_fit(size_t size) {
    std::lock_guard<std::mutex> guard(get_mutex());
    const size_t total_required_size = size + occupied_block_metadata_size;

    size_t allocator_total_size = *reinterpret_cast<size_t*>(static_cast<char*>(_trusted_memory) +
                                sizeof(logger*) + sizeof(memory_resource*) +
                                sizeof(allocator_with_fit_mode::fit_mode));

    char* heap_start = static_cast<char*>(_trusted_memory) + allocator_metadata_size;
    char* heap_end = heap_start + allocator_total_size;
    void** first_block_ptr = reinterpret_cast<void**>(heap_start - sizeof(void*));

    void* worst_block = nullptr;
    void* worst_prev_block = nullptr;
    void* worst_next_block = nullptr;
    size_t worst_hole_size = 0;

    if (*first_block_ptr == nullptr) {
        if (heap_end - heap_start >= total_required_size) {
            return allocate_new_block(heap_start, size, first_block_ptr, heap_end - heap_start);
        }
        return nullptr;
    }

    size_t front_hole_size = static_cast<char*>(*first_block_ptr) - heap_start;
    if (front_hole_size >= total_required_size) {
        worst_hole_size = front_hole_size;
        worst_block = static_cast<void*>(heap_start);
        worst_next_block = *first_block_ptr;
        worst_prev_block = nullptr;
    }

    void* current_block = *first_block_ptr;
    while (current_block != nullptr) {
        size_t current_block_size = *reinterpret_cast<size_t*>(current_block);
        void* next_block = *reinterpret_cast<void**>(static_cast<char*>(current_block) + sizeof(size_t));
        char* current_block_end = static_cast<char*>(current_block) + occupied_block_metadata_size + current_block_size;

        size_t hole_size = (next_block != nullptr) ? static_cast<char*>(next_block) - current_block_end : heap_end - current_block_end;

        if (hole_size >= total_required_size && hole_size > worst_hole_size) {
            worst_block = current_block_end;
            worst_prev_block = current_block;
            worst_next_block = next_block;
            worst_hole_size = hole_size;
        }

        current_block = next_block;
    }

    if (worst_block != nullptr) {
        return allocate_in_hole(reinterpret_cast<char*>(worst_block), size, first_block_ptr,
                              worst_prev_block, worst_next_block, worst_hole_size);
    }
    return nullptr;
}

void* allocator_boundary_tags::allocate_new_block(char* address, size_t size, void** first_block_ptr, size_t free_size) {
    if (free_size - size - occupied_block_metadata_size < occupied_block_metadata_size) {
        size += free_size - size - occupied_block_metadata_size;
    }

    *reinterpret_cast<size_t*>(address) = size;
    *reinterpret_cast<void**>(address + sizeof(size_t)) = nullptr;
    *reinterpret_cast<void**>(address + sizeof(size_t) + sizeof(void*)) = nullptr;
    *reinterpret_cast<void**>(address + sizeof(size_t) + 2 * sizeof(void*)) = _trusted_memory;
    *first_block_ptr = address;

    return address;
}

void* allocator_boundary_tags::allocate_in_hole(char* address, size_t size, void** first_block_ptr,
                                              void* prev_block, void* next_block, size_t free_size) {
    if (free_size - size - occupied_block_metadata_size < occupied_block_metadata_size) {
        size += free_size - size - occupied_block_metadata_size;
    }

    *reinterpret_cast<size_t*>(address) = size;
    *reinterpret_cast<void**>(address + sizeof(size_t)) = next_block;
    *reinterpret_cast<void**>(address + sizeof(size_t) + sizeof(void*)) = prev_block;
    *reinterpret_cast<void**>(address + sizeof(size_t) + 2 * sizeof(void*)) = _trusted_memory;

    if (prev_block != nullptr) {
        *reinterpret_cast<void**>(static_cast<char*>(prev_block) + sizeof(size_t)) = address;
    } else {
        *first_block_ptr = address;
    }

    if (next_block != nullptr) {
        *reinterpret_cast<void**>(static_cast<char*>(next_block) + sizeof(size_t) + sizeof(void*)) = address;
    }

    return address;
}

