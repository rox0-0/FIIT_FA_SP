#include "../include/allocator_buddies_system.h"
#include <cstring>
allocator_buddies_system::allocator_buddies_system(size_t space_size, std::pmr::memory_resource *parent_allocator,
												   logger *logger,
												   allocator_with_fit_mode::fit_mode allocate_fit_mode) {
	if (logger) logger->debug("Старт конструктора");
	if (space_size < min_k) {
		if (logger) logger->error("Размер должен быть больше min_k");
		throw std::logic_error("запрашиваемый размер слишком мал");
	}
	try {
		size_t space_size_power = __detail::nearest_greater_k_of_2(space_size);
		if (!parent_allocator) {
			parent_allocator = std::pmr::get_default_resource();
		}

		size_t total_size = (1ULL << space_size_power) + allocator_metadata_size;
		_trusted_memory = parent_allocator->allocate(total_size);

		auto *memory = reinterpret_cast<uint8_t *>(_trusted_memory);

		*reinterpret_cast<class logger **>(memory) = logger;
		memory += sizeof(class logger *);

		*reinterpret_cast<allocator_dbg_helper **>(memory) = nullptr;
		memory += sizeof(allocator_dbg_helper *);

		*reinterpret_cast<allocator_with_fit_mode::fit_mode *>(memory) = allocate_fit_mode;
		memory += sizeof(allocator_with_fit_mode::fit_mode);

		*reinterpret_cast<unsigned char *>(memory) = static_cast<unsigned char>(space_size_power);
		memory += sizeof(unsigned char);

		new (reinterpret_cast<std::mutex *>(memory)) std::mutex();
		memory += sizeof(std::mutex);

		auto first_block = reinterpret_cast<block_metadata *>(memory);
		first_block->occupied = false;
		first_block->size = static_cast<unsigned char>(space_size_power);
	} catch (const std::exception &e) {
		if (logger) logger->error("ошибка инициализации аллокатора");
		throw std::iostream ::failure("ошибка инициализации аллокатора");
	}

	if (logger) logger->debug("конец конструктора");
}

allocator_buddies_system::~allocator_buddies_system() {
	logger *logger_instance = get_logger();
	if (logger_instance) logger_instance->debug("начало удаления аллокатора ");
	if (!_trusted_memory) return;
	try {
		unsigned char space_size_power = *reinterpret_cast<unsigned char *>(
				reinterpret_cast<uint8_t *>(_trusted_memory) + allocator_metadata_size);

		size_t total_size = (1ULL << space_size_power) + allocator_metadata_size;
		std::pmr::memory_resource *parent_allocator = std::pmr::get_default_resource();

		auto mutex_ptr = reinterpret_cast<std::mutex *>(reinterpret_cast<uint8_t *>(_trusted_memory) + allocator_metadata_size);
		mutex_ptr->~mutex();

		parent_allocator->deallocate(_trusted_memory, total_size);
	} catch (const std::exception &ex) {
		if (logger_instance) logger_instance->error("ошибка при удалении аллокатора: " + std::string(ex.what()));
		_trusted_memory = nullptr;
	}

	if (logger_instance) logger_instance->trace("конец удаления аллокатора ");
}

allocator_buddies_system::allocator_buddies_system(allocator_buddies_system &&other) noexcept
	: _trusted_memory(std::exchange(other._trusted_memory, nullptr)) {
	trace_with_guard("начало конструктора");

	std::lock_guard<std::mutex> lock(other.get_mutex());
	other._trusted_memory = nullptr;

	get_logger()->debug("ресурсы перемещены");
	trace_with_guard("конец консттруктора");
}

allocator_buddies_system &allocator_buddies_system::operator=(allocator_buddies_system &&other) noexcept {
	if (this != &other) {
		this->~allocator_buddies_system();
		_trusted_memory = std::exchange(other._trusted_memory, nullptr);
	}

	return *this;
}

[[nodiscard]] void *allocator_buddies_system::do_allocate_sm(size_t size) {
	logger *logger = get_logger();
	if (logger) logger->debug("старт аллокации");
	std::lock_guard lock(get_mutex());
	if (size == 0) size = 1;

	size_t required_k = __detail::nearest_greater_k_of_2(size + sizeof(block_metadata));
	required_k = std::max(required_k, min_k);

	void *target_block = find_suitable_block(required_k, get_fit_mode());
	if (!target_block) {
		if (logger) logger->error("фейл аллокации для байт в размере " + std::to_string(size));
		throw std::bad_alloc();
	}

	auto meta = reinterpret_cast<block_metadata *>(target_block);
	while (meta->size > required_k) {
		meta->size--;
		void *buddy = reinterpret_cast<uint8_t *>(target_block) + (1ULL << meta->size);
		auto buddy_meta = reinterpret_cast<block_metadata *>(buddy);
		buddy_meta->occupied = false;
		buddy_meta->size = meta->size;
	}

	meta->occupied = true;

	if (logger) logger->debug("конец аллокации");
	return reinterpret_cast<uint8_t *>(target_block) + sizeof(block_metadata);
}

void allocator_buddies_system::do_deallocate_sm(void *at) {
	logger *logger = get_logger();
	if (logger) logger->debug("начало деаллокации");
	std::lock_guard<std::mutex> guard(get_mutex());

	if (!at) return;

	void *block = reinterpret_cast<uint8_t *>(at) - sizeof(block_metadata);
	auto meta = reinterpret_cast<block_metadata *>(block);

	if (!meta->occupied) {
		if (logger) logger->error("два свободных");
		throw std::logic_error("два свободных");
	}

	meta->occupied = false;

	unsigned char max_k = get_space_size_power();
	void *allocator_start = reinterpret_cast<uint8_t *>(_trusted_memory) + allocator_metadata_size;
	size_t allocator_size = 1ULL << max_k;
	void *allocator_end = reinterpret_cast<uint8_t *>(allocator_start) + allocator_size;

	while (meta->size < max_k) {
		size_t block_size = 1ULL << meta->size;
		uintptr_t buddy_addr = reinterpret_cast<uintptr_t>(block) ^ block_size;

		if (buddy_addr < reinterpret_cast<uintptr_t>(allocator_start) || buddy_addr >= reinterpret_cast<uintptr_t>(allocator_end)) {
			break;
		}

		auto buddy_meta = reinterpret_cast<block_metadata *>(buddy_addr);
		if (buddy_meta->occupied || buddy_meta->size != meta->size) {
			break;
		}

		if (buddy_addr < reinterpret_cast<uintptr_t>(block)) {
			block = reinterpret_cast<void *>(buddy_addr);
			meta = buddy_meta;
		}

		meta->size++;
	}

	if (logger) logger->debug("деаллокация завершена");
}

allocator_buddies_system::allocator_buddies_system(const allocator_buddies_system &other) {
	trace_with_guard("старт конструктора");
	unsigned char space_size_power = *reinterpret_cast<unsigned char *>(reinterpret_cast<uint8_t *>(other._trusted_memory) +
																		sizeof(logger *) +
																		sizeof(allocator_dbg_helper *) +
																		sizeof(fit_mode));

	size_t total_size = (1ULL << space_size_power) + allocator_metadata_size;
	std::pmr::memory_resource *parent_allocator = std::pmr::get_default_resource();

	_trusted_memory = parent_allocator->allocate(total_size);
	std::memcpy(_trusted_memory, other._trusted_memory, total_size);

	new (reinterpret_cast<uint8_t *>(_trusted_memory) +
		 sizeof(logger *) +
		 sizeof(allocator_dbg_helper *) +
		 sizeof(fit_mode) +
		 sizeof(unsigned char)) std::mutex;

	trace_with_guard("конец конструктора");
}

allocator_buddies_system &allocator_buddies_system::operator=(const allocator_buddies_system &other) {
	if (this != &other) {
		this->~allocator_buddies_system();
		new (this) allocator_buddies_system(other);
	}

	return *this;
}

bool allocator_buddies_system::do_is_equal(const std::pmr::memory_resource &other) const noexcept {
	if (this == &other) return true;

	const auto *derived = dynamic_cast<const allocator_buddies_system *>(&other);

	if (!derived) return false;

	return this->_trusted_memory == derived->_trusted_memory;
}

inline void allocator_buddies_system::set_fit_mode(allocator_with_fit_mode::fit_mode mode) {
	*reinterpret_cast<fit_mode *>(reinterpret_cast<uint8_t *>(_trusted_memory) +
								  sizeof(logger *) +
								  sizeof(allocator_dbg_helper *)) = mode;
}


std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info() const noexcept {
	logger *logger = get_logger();
	if (logger) logger->trace("начало сбора инфы");
	std::vector<block_info> res;
	if (_trusted_memory) {
		res.reserve(1 << (get_space_size_power() - min_k));
	}

	for (auto it = begin(), end_it = end(); it != end_it; ++it) {
		res.push_back({.block_size = static_cast<size_t>(1) << it.size(), .is_block_occupied = it.occupied()});
	}

	if (logger) logger->trace("конец сбора инфы");
	return res;
}

inline logger *allocator_buddies_system::get_logger() const {
	if (!_trusted_memory) return nullptr;
	return *reinterpret_cast<logger **>(_trusted_memory);
}

inline std::string allocator_buddies_system::get_typename() const {
	return "allocator_buddies_system";
}

std::vector<allocator_test_utils::block_info> allocator_buddies_system::get_blocks_info_inner() const {
	return get_blocks_info();
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::begin() const noexcept {
	void *first_block = reinterpret_cast<uint8_t *>(_trusted_memory) + allocator_metadata_size;
	return {first_block};
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::end() const noexcept {
	unsigned char max_k = *reinterpret_cast<unsigned char *>(
			reinterpret_cast<uint8_t *>(_trusted_memory) +
			sizeof(logger *) +
			sizeof(allocator_dbg_helper *) +
			sizeof(fit_mode));

	size_t allocator_size = 1 << max_k;
	void *allocator_end = reinterpret_cast<uint8_t *>(_trusted_memory) + allocator_metadata_size + allocator_size;

	return {allocator_end};
}

bool allocator_buddies_system::buddy_iterator::operator==(const allocator_buddies_system::buddy_iterator &other) const noexcept {
	return _block == other._block;
}

bool allocator_buddies_system::buddy_iterator::operator!=(const allocator_buddies_system::buddy_iterator &other) const noexcept {
	return _block != other._block;
}

allocator_buddies_system::buddy_iterator &allocator_buddies_system::buddy_iterator::operator++() & noexcept {
	if (!_block) return *this;

	auto meta = reinterpret_cast<block_metadata *>(_block);
	size_t block_size = 1 << meta->size;
	_block = reinterpret_cast<uint8_t *>(_block) + block_size;

	return *this;
}

allocator_buddies_system::buddy_iterator allocator_buddies_system::buddy_iterator::operator++(int) {
	buddy_iterator tmp = *this;
	++(*this);
	return tmp;
}

size_t allocator_buddies_system::buddy_iterator::size() const noexcept {
	if (!_block) return 0;
	return reinterpret_cast<block_metadata *>(_block)->size;
}

bool allocator_buddies_system::buddy_iterator::occupied() const noexcept {
	if (!_block) return false;
	return reinterpret_cast<block_metadata *>(_block)->occupied;
}

void *allocator_buddies_system::buddy_iterator::operator*() const noexcept {
	return _block;
}

allocator_buddies_system::buddy_iterator::buddy_iterator(void *start) : _block(start) {}

allocator_buddies_system::buddy_iterator::buddy_iterator() : _block(nullptr) {}

std::mutex &allocator_buddies_system::get_mutex() const {
	if (!_trusted_memory) throw std::logic_error("мьютекс не освобожден");

	return *reinterpret_cast<std::mutex *>(reinterpret_cast<uint8_t *>(_trusted_memory) +
										   sizeof(logger *) +
										   sizeof(allocator_dbg_helper *) +
										   sizeof(fit_mode) +
										   sizeof(unsigned char));
}

allocator_with_fit_mode::fit_mode &allocator_buddies_system::get_fit_mode() const noexcept {
	return *reinterpret_cast<fit_mode *>(reinterpret_cast<uint8_t *>(_trusted_memory) +
										 sizeof(logger *) +
										 sizeof(allocator_dbg_helper *));
}

unsigned char allocator_buddies_system::get_space_size_power() const {
	if (!_trusted_memory) throw std::runtime_error("аллокатор не инициализирован");

	return *reinterpret_cast<unsigned char *>(reinterpret_cast<uint8_t *>(_trusted_memory) +
											  sizeof(logger *) + sizeof(allocator_dbg_helper *) + sizeof(fit_mode));
}

void *allocator_buddies_system::find_suitable_block(size_t required_k,
													allocator_with_fit_mode::fit_mode mode) const {
	void *target_block = nullptr;
	size_t best_diff = SIZE_MAX;
	size_t worst_size = 0;

	for (auto it = begin(); it != end(); ++it) {
		if (!it.occupied() && it.size() >= required_k) {
			size_t current_size = it.size();

			if (mode == fit_mode::first_fit) {
				return *it;
			} else if (mode == fit_mode::the_best_fit) {
				size_t diff = current_size - required_k;
				if (diff == 0) return *it;
				if (diff < best_diff) {
					target_block = *it;
					best_diff = diff;
				}
			} else if (mode == fit_mode::the_worst_fit) {
				if (current_size > worst_size) {
					target_block = *it;
					worst_size = current_size;
				}
			}
		}
	}

	return target_block;
}