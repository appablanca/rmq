#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <limits>
#include <utility>
#include <unordered_map>
#include <cmath>

inline size_t floor_log2(size_t x)
{
	return 63 - static_cast<size_t>(__builtin_clzll(static_cast<unsigned long long>(x)));
}
// RMQ interface (duck-typed via templates):
//
//   static std::string name();
//   static size_t max_n();               // optional, defaults to SIZE_MAX
//   static RMQ build(const std::vector<uint64_t>& data);
//   size_t space() const;
//   uint64_t query(size_t l, size_t r) const;

struct OnTheFlyNaive
{
	static std::string name() { return "NaiFly"; }
	static size_t max_n() { return 10'000; }
	const std::vector<uint64_t> *data;

	static OnTheFlyNaive build(const std::vector<uint64_t> &data) { return {&data}; }

	size_t space() const { return sizeof(*this); }

	uint64_t query(size_t l, size_t r) const
	{
		uint64_t min = (*data)[l];
		for (size_t i = l + 1; i <= r; ++i)
			min = std::min(min, (*data)[i]);
		return min;
	}
};

struct PrecomputedNaive
{
	static std::string name() { return "NaiPre"; }

	static size_t max_n() { return 10'000; }

	const std::vector<uint64_t> *data = nullptr;

	size_t n = 0;

	std::vector<uint32_t> table;

	size_t index(size_t l, size_t r) const
	{
		return l * n - (l * (l - 1)) / 2 + (r - l);
	}

	static PrecomputedNaive build(const std::vector<uint64_t> &data)
	{
		const size_t n = data.size();
		PrecomputedNaive rmq;

		rmq.data = &data;
		rmq.n = n;
		rmq.table.resize(n * (n + 1) / 2);

		for (size_t l = 0; l < n; ++l)
		{
			uint32_t min_pos = static_cast<uint32_t>(l);

			for (size_t r = l; r < n; ++r)
			{
				if (data[r] < data[min_pos])
				{
					min_pos = static_cast<uint32_t>(r);
				}

				rmq.table[rmq.index(l, r)] = min_pos;
			}
		}
		return rmq;
	}

	size_t space() const
	{
		size_t total = sizeof(*this);

		total += table.capacity() * sizeof(uint32_t);

		return total;
	}

	uint64_t query(size_t l, size_t r) const
	{
		return (*data)[table[index(l, r)]];
	}
};

struct SparseTable
{
	static std::string name() { return "SpTa"; }
	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;
	std::vector<std::vector<uint32_t>> table;

	static SparseTable build(const std::vector<uint64_t> &data)
	{
		const size_t n = data.size();
		const size_t k = static_cast<size_t>(floor_log2(n));

		SparseTable rmq;
		rmq.data = &data;

		rmq.table.resize(k + 1);

		rmq.table[0].resize(n);

		for (size_t i = 0; i < n; i++)
		{
			rmq.table[0][i] = i;
		}

		for (size_t level = 1; level <= k; level++)
		{
			size_t len = 1ULL << level;
			size_t half = 1ULL << (level - 1);
			size_t L = n + 1 - len;

			rmq.table[level].resize(L);

			for (size_t j = 0; j < L; j++)
			{
				size_t left_idx = rmq.table[level - 1][j];
				size_t right_idx = rmq.table[level - 1][j + half];

				if (data[left_idx] <= data[right_idx])
					rmq.table[level][j] = left_idx;
				else
					rmq.table[level][j] = right_idx;
			}
		}

		return rmq;
	}

	size_t space() const
	{
		size_t bytes = sizeof(*this);

		for (const auto &row : table)
		{
			bytes += row.capacity() * sizeof(uint32_t);
		}

		return bytes;
	}

	size_t query_index(size_t l, size_t r) const
	{
		size_t len = r - l + 1;
		size_t k = static_cast<size_t>(floor_log2(len));

		size_t left_idx = table[k][l];
		size_t right_idx = table[k][r + 1 - (1ULL << k)];

		if ((*data)[left_idx] <= (*data)[right_idx])
			return left_idx;
		else
			return right_idx;
	}

	uint64_t query(size_t l, size_t r) const
	{
		return (*data)[query_index(l, r)];
	}
};

struct SegmentTree
{
	static std::string name() { return "SeTr"; }

	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;
	std::vector<std::vector<uint32_t>> tree;

	static SegmentTree build(const std::vector<uint64_t> &data)
	{
		const size_t n = data.size();
		const size_t k = static_cast<uint64_t>(floor_log2(n));

		SegmentTree rmq;

		rmq.data = &data;

		rmq.tree.resize(k + 1);
		rmq.tree[0].resize(n);

		for (uint64_t i = 0; i < n; i++)
		{
			rmq.tree[0][i] = i;
		}

		for (uint64_t level = 1; level <= k; level++)
		{

			size_t L = static_cast<size_t>(n / (size_t(1) << level));
			rmq.tree[level].resize(L);
			for (size_t j = 0; j < L; j++)
			{
				size_t left_idx = rmq.tree[level - 1][2 * j];
				size_t right_idx = rmq.tree[level - 1][2 * j + 1];

				if (data[left_idx] <= data[right_idx])
					rmq.tree[level][j] = left_idx;
				else
					rmq.tree[level][j] = right_idx;
			}
		}

		return rmq;
	}

	size_t space() const
	{

		size_t bytes = sizeof(*this);

		for (const auto &row : tree)
		{
			bytes += row.capacity() * sizeof(uint32_t);
		}

		return bytes;
	}

	size_t query_index(size_t l, size_t r) const
	{
		size_t best_idx = SIZE_MAX;

		size_t level = 0;
		size_t block = 1;

		while (l <= r)
		{
			size_t period = 2 * block;

			if (l % period == block)
			{
				size_t idx = tree[level][l / block];

				if (best_idx == SIZE_MAX || (*data)[idx] < (*data)[best_idx])
					best_idx = idx;

				l += block;

				if (l > r)
					break;
			}

			if ((r + 1) % period == block)
			{
				size_t idx = tree[level][r / block];

				if (best_idx == SIZE_MAX || (*data)[idx] < (*data)[best_idx])
					best_idx = idx;

				if (r < block)
				{
					break;
				}

				r -= block;

				if (l > r)
					break;
			}
			level++;
			block <<= 1;
			if (level >= tree.size())
				break;
		}

		return best_idx;
	}

	uint64_t query(size_t l, size_t r) const
	{
		return (*data)[query_index(l, r)];
	}
};

template <size_t BlockSize>
struct Blocks
{
	static std::string name() { return "FiBl" + std::to_string(BlockSize); }

	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;
	std::vector<std::vector<uint32_t>> table;

	static Blocks build(const std::vector<uint64_t> &data)
	{
		Blocks rmq;
		rmq.data = &data;

		size_t n = rmq.data->size();
		size_t amountOfBlocks = (n + BlockSize - 1) / BlockSize;

		size_t numberOfLevels = 0;
		for (size_t x = amountOfBlocks; x > 0; x >>= 1)
		{

			++numberOfLevels;
		}

		rmq.table.resize(numberOfLevels);

		rmq.table[0].resize(amountOfBlocks);

		// level 0
		for (size_t block = 0; block < amountOfBlocks; ++block)
		{
			const size_t start = block * BlockSize;
			const size_t end = std::min(start + BlockSize, n);

			size_t blockMin = start;

			for (size_t j = start + 1; j < end; ++j)
			{
				if (data[j] < data[blockMin])

					blockMin = j;
			}
			rmq.table[0][block] = blockMin;
		}

		// level 1+ sparse table
		for (size_t level = 1; level < numberOfLevels; level++)
		{
			size_t len = 1ULL << level;
			size_t half = 1ULL << (level - 1);
			size_t L = amountOfBlocks + 1 - len;

			rmq.table[level].resize(L);

			for (size_t j = 0; j < L; ++j)
			{
				size_t left = rmq.table[level - 1][j];
				size_t right = rmq.table[level - 1][j + half];
				if (data[left] <= data[right])
					rmq.table[level][j] = left;
				else
					rmq.table[level][j] = right;
			}
		}
		return rmq;
	}

	uint64_t query(size_t l, size_t r) const
	{
		const uint64_t *p = data->data();
		const size_t bl = l / BlockSize, br = r / BlockSize;

		if (bl == br)
		{
			uint64_t ans = p[l];
			for (size_t i = l + 1; i <= r; ++i)
				ans = std::min(ans, p[i]);
			return ans;
		}

		uint64_t ans = p[l];
		for (size_t i = l + 1; i < (bl + 1) * BlockSize; ++i)
			ans = std::min(ans, p[i]);

		for (size_t i = br * BlockSize; i <= r; ++i)
			ans = std::min(ans, p[i]);

		const size_t lo = bl + 1, hi = br - 1;
		if (lo <= hi)
		{
			const size_t k = floor_log2(hi - lo + 1);
			ans = std::min(ans, std::min(p[table[k][lo]], p[table[k][hi + 1 - (1ULL << k)]]));
		}
		return ans;
	}

	size_t space() const
	{
		size_t total = sizeof(*this);

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(uint32_t);

		return total;
	}
};

template <size_t CCOMP>
struct AdaptiveBlocks
{
	size_t blockSize;

	static std::string name()
	{
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2)
			<< (CCOMP / 100.0);
		return "AdBl" + oss.str();
	}

	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;
	std::vector<std::vector<uint32_t>> table;

	static AdaptiveBlocks build(const std::vector<uint64_t> &data)
	{
		AdaptiveBlocks rmq;

		constexpr double C = CCOMP / 100.0;

		rmq.blockSize = std::max<size_t>(
			1,
			static_cast<size_t>(C * std::log2(data.size())));

		rmq.data = &data;

		size_t n = data.size();
		size_t amountOfBlocks = (n + rmq.blockSize - 1) / rmq.blockSize;

		size_t numberOfLevels = 0;
		for (size_t x = amountOfBlocks; x > 0; x >>= 1)
			++numberOfLevels;

		rmq.table.resize(numberOfLevels);
		rmq.table[0].resize(amountOfBlocks);

		// level 0
		for (size_t block = 0; block < amountOfBlocks; ++block)
		{
			size_t start = block * rmq.blockSize;
			size_t end = std::min(start + rmq.blockSize, n);

			size_t blockMin = start;

			for (size_t j = start + 1; j < end; ++j)
			{
				if (data[j] < data[blockMin])
					blockMin = j;
			}

			rmq.table[0][block] = blockMin;
		}

		// sparse table
		for (size_t level = 1; level < numberOfLevels; ++level)
		{
			size_t len = 1ULL << level;
			size_t half = len >> 1;
			size_t L = amountOfBlocks + 1 - len;

			rmq.table[level].resize(L);

			for (size_t j = 0; j < L; ++j)
			{
				size_t left = rmq.table[level - 1][j];
				size_t right = rmq.table[level - 1][j + half];

				if (data[left] <= data[right])
					rmq.table[level][j] = left;
				else
					rmq.table[level][j] = right;
			}
		}

		return rmq;
	}

	uint64_t query(size_t l, size_t r) const
	{
		const uint64_t *p = data->data();
		const size_t bl = l / blockSize, br = r / blockSize;

		if (bl == br)
		{
			uint64_t ans = p[l];
			for (size_t i = l + 1; i <= r; ++i)
				ans = std::min(ans, p[i]);
			return ans;
		}

		uint64_t ans = p[l]; // tail of block bl
		for (size_t i = l + 1; i < (bl + 1) * blockSize; ++i)
			ans = std::min(ans, p[i]);

		for (size_t i = br * blockSize; i <= r; ++i)
			ans = std::min(ans, p[i]);

		const size_t lo = bl + 1, hi = br - 1;
		if (lo <= hi)
		{
			const size_t k = floor_log2(hi - lo + 1);
			ans = std::min(ans, std::min(p[table[k][lo]], p[table[k][hi + 1 - (1ULL << k)]]));
		}
		return ans;
	}

	size_t space() const
	{
		size_t total = sizeof(*this);

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(uint32_t);

		return total;
	}
};

template <size_t BlockSize>
struct BlocksPrecompute
{
	static std::string name() { return "FiBlPre" + std::to_string(BlockSize); }

	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;

	std::vector<std::vector<uint32_t>> table;

	std::vector<uint32_t> prefixMin;
	std::vector<uint32_t> suffixMin;

	static BlocksPrecompute build(const std::vector<uint64_t> &data)
	{

		BlocksPrecompute rmq;
		rmq.data = &data;

		size_t n = data.size();

		size_t amountOfBlocks = (n + BlockSize - 1) / BlockSize;

		size_t numberOfLevels = 0;
		for (size_t x = amountOfBlocks; x > 0; x >>= 1)
			++numberOfLevels;

		rmq.table.resize(numberOfLevels);
		rmq.table[0].resize(amountOfBlocks);

		rmq.prefixMin.resize(n);
		rmq.suffixMin.resize(n);

		// level 0 + prefix/suffix minima
		for (size_t block = 0; block < amountOfBlocks; ++block)
		{
			size_t start = block * BlockSize;
			size_t end = std::min(start + BlockSize, n);

			// prefix minima
			rmq.prefixMin[start] = static_cast<uint32_t>(start);
			for (size_t i = start + 1; i < end; ++i)
			{
				uint32_t prev = rmq.prefixMin[i - 1];
				if (data[i] < data[prev])
					rmq.prefixMin[i] = static_cast<uint32_t>(i);
				else
					rmq.prefixMin[i] = prev;
			}

			// block minimum == prefix minimum over the whole block
			rmq.table[0][block] = static_cast<size_t>(rmq.prefixMin[end - 1]);

			// suffix minima
			rmq.suffixMin[end - 1] = static_cast<uint32_t>(end - 1);
			for (size_t i = end - 1; i > start; --i)
			{
				uint32_t next = rmq.suffixMin[i];
				if (data[i - 1] <= data[next])
					rmq.suffixMin[i - 1] = static_cast<uint32_t>(i - 1);
				else
					rmq.suffixMin[i - 1] = next;
			}
		}
		// sparse table
		for (size_t level = 1; level < numberOfLevels; ++level)
		{
			size_t len = 1ULL << level;
			size_t half = len >> 1;
			size_t L = amountOfBlocks + 1 - len;

			rmq.table[level].resize(L);

			for (size_t j = 0; j < L; ++j)
			{
				size_t left = rmq.table[level - 1][j];
				size_t right = rmq.table[level - 1][j + half];

				if (data[left] <= data[right])
					rmq.table[level][j] = left;
				else
					rmq.table[level][j] = right;
			}
		}

		return rmq;
	}

	uint64_t query(size_t l, size_t r) const
	{
		const uint64_t *p = data->data();
		const size_t bl = l / BlockSize, br = r / BlockSize;

		if (bl == br)
		{
			uint64_t ans = p[l];
			for (size_t i = l + 1; i <= r; ++i)
				ans = std::min(ans, p[i]);
			return ans;
		}

		uint64_t ans = std::min(p[suffixMin[l]], p[prefixMin[r]]);

		const size_t lo = bl + 1, hi = br - 1;
		if (lo <= hi)
		{
			const size_t k = floor_log2(hi - lo + 1);
			ans = std::min(ans, std::min(p[table[k][lo]], p[table[k][hi + 1 - (1ULL << k)]]));
		}
		return ans;
	}
	size_t space() const
	{
		size_t total = sizeof(*this);

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(uint32_t);

		total += prefixMin.capacity() * sizeof(uint32_t);

		total += suffixMin.capacity() * sizeof(uint32_t);

		return total;
	}
};

template <size_t CCOMP>
struct AdaptiveBlocksPrecompute
{
	size_t blockSize;

	static std::string name()
	{
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2)
			<< (CCOMP / 100.0);
		return "AdBlPre" + oss.str();
	}

	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;

	std::vector<std::vector<uint32_t>> table;

	std::vector<uint32_t> prefixMin;
	std::vector<uint32_t> suffixMin;

	static AdaptiveBlocksPrecompute build(const std::vector<uint64_t> &data)
	{
		AdaptiveBlocksPrecompute rmq;

		constexpr double C = CCOMP / 100.0;

		rmq.blockSize = std::max<size_t>(
			1,
			static_cast<size_t>(C * std::log2(data.size())));
		rmq.data = &data;

		size_t n = data.size();

		size_t amountOfBlocks = (n + rmq.blockSize - 1) / rmq.blockSize;

		size_t numberOfLevels = 0;
		for (size_t x = amountOfBlocks; x > 0; x >>= 1)
			++numberOfLevels;

		rmq.table.resize(numberOfLevels);
		rmq.table[0].resize(amountOfBlocks);

		rmq.prefixMin.resize(n);
		rmq.suffixMin.resize(n);

		// level 0 + prefix/suffix minima
		for (size_t block = 0; block < amountOfBlocks; ++block)
		{
			size_t start = block * rmq.blockSize;
			size_t end = std::min(start + rmq.blockSize, n);

			// prefix minima
			rmq.prefixMin[start] = static_cast<uint32_t>(start);
			for (size_t i = start + 1; i < end; ++i)
			{
				uint32_t prev = rmq.prefixMin[i - 1];
				if (data[i] < data[prev])
					rmq.prefixMin[i] = static_cast<uint32_t>(i);
				else
					rmq.prefixMin[i] = prev;
			}

			// block minimum == prefix minimum over the whole block
			rmq.table[0][block] = static_cast<size_t>(rmq.prefixMin[end - 1]);

			// suffix minima
			rmq.suffixMin[end - 1] = static_cast<uint32_t>(end - 1);
			for (size_t i = end - 1; i > start; --i)
			{
				uint32_t next = rmq.suffixMin[i];
				if (data[i - 1] <= data[next])
					rmq.suffixMin[i - 1] = static_cast<uint32_t>(i - 1);
				else
					rmq.suffixMin[i - 1] = next;
			}
		}
		// sparse table
		for (size_t level = 1; level < numberOfLevels; ++level)
		{
			size_t len = 1ULL << level;
			size_t half = len >> 1;
			size_t L = amountOfBlocks + 1 - len;

			rmq.table[level].resize(L);

			for (size_t j = 0; j < L; ++j)
			{
				size_t left = rmq.table[level - 1][j];
				size_t right = rmq.table[level - 1][j + half];

				if (data[left] <= data[right])
					rmq.table[level][j] = left;
				else
					rmq.table[level][j] = right;
			}
		}

		return rmq;
	}

	uint64_t query(size_t l, size_t r) const
	{
		const uint64_t *p = data->data();
		const size_t bl = l / blockSize, br = r / blockSize;

		if (bl == br)
		{
			uint64_t ans = p[l];
			for (size_t i = l + 1; i <= r; ++i)
				ans = std::min(ans, p[i]);
			return ans;
		}

		uint64_t ans = std::min(p[suffixMin[l]], p[prefixMin[r]]);

		const size_t lo = bl + 1, hi = br - 1;
		if (lo <= hi)
		{
			const size_t k = floor_log2(hi - lo + 1);
			ans = std::min(ans, std::min(p[table[k][lo]], p[table[k][hi + 1 - (1ULL << k)]]));
		}
		return ans;
	}

	size_t space() const
	{
		size_t total = sizeof(*this);

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(uint32_t);

		total += prefixMin.capacity() * sizeof(uint32_t);
		total += suffixMin.capacity() * sizeof(uint32_t);

		return total;
	}
};

template <size_t CCOMP>
struct SqrtAdaptiveBlocksPrecompute
{
	size_t blockSize;

	static std::string name()
	{
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2)
			<< (CCOMP / 100.0);
		return "SqBlPre" + oss.str();
	}

	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;

	std::vector<std::vector<uint32_t>> table;

	std::vector<uint32_t> prefixMin;
	std::vector<uint32_t> suffixMin;

	static SqrtAdaptiveBlocksPrecompute build(const std::vector<uint64_t> &data)
	{
		SqrtAdaptiveBlocksPrecompute rmq;

		constexpr double C = CCOMP / 100.0;

		rmq.blockSize = std::max<size_t>(
			1,
			static_cast<size_t>(C * std::sqrt(data.size())));
		rmq.data = &data;

		size_t n = data.size();
		size_t amountOfBlocks = (n + rmq.blockSize - 1) / rmq.blockSize;

		size_t numberOfLevels = 0;
		for (size_t x = amountOfBlocks; x > 0; x >>= 1)
			++numberOfLevels;

		rmq.table.resize(numberOfLevels);
		rmq.table[0].resize(amountOfBlocks);

		rmq.prefixMin.resize(n);
		rmq.suffixMin.resize(n);

		// level 0 + prefix/suffix minima
		for (size_t block = 0; block < amountOfBlocks; ++block)
		{
			size_t start = block * rmq.blockSize;
			size_t end = std::min(start + rmq.blockSize, n);

			// prefix minima
			rmq.prefixMin[start] = static_cast<uint32_t>(start);
			for (size_t i = start + 1; i < end; ++i)
			{
				uint32_t prev = rmq.prefixMin[i - 1];
				if (data[i] < data[prev])
					rmq.prefixMin[i] = static_cast<uint32_t>(i);
				else
					rmq.prefixMin[i] = prev;
			}

			// block minimum == prefix minimum over the whole block
			rmq.table[0][block] = static_cast<size_t>(rmq.prefixMin[end - 1]);

			// suffix minima
			rmq.suffixMin[end - 1] = static_cast<uint32_t>(end - 1);
			for (size_t i = end - 1; i > start; --i)
			{
				uint32_t next = rmq.suffixMin[i];
				if (data[i - 1] <= data[next])
					rmq.suffixMin[i - 1] = static_cast<uint32_t>(i - 1);
				else
					rmq.suffixMin[i - 1] = next;
			}
		}

		// sparse table
		for (size_t level = 1; level < numberOfLevels; ++level)
		{
			size_t len = 1ULL << level;
			size_t half = len >> 1;
			size_t L = amountOfBlocks + 1 - len;

			rmq.table[level].resize(L);

			for (size_t j = 0; j < L; ++j)
			{
				size_t left = rmq.table[level - 1][j];
				size_t right = rmq.table[level - 1][j + half];

				if (data[left] <= data[right])
					rmq.table[level][j] = left;
				else
					rmq.table[level][j] = right;
			}
		}

		return rmq;
	}

	uint64_t query(size_t l, size_t r) const
	{
		const uint64_t *p = data->data();
		const size_t bl = l / blockSize, br = r / blockSize;

		if (bl == br)
		{
			uint64_t ans = p[l];
			for (size_t i = l + 1; i <= r; ++i)
				ans = std::min(ans, p[i]);
			return ans;
		}

		uint64_t ans = std::min(p[suffixMin[l]], p[prefixMin[r]]);

		const size_t lo = bl + 1, hi = br - 1;
		if (lo <= hi)
		{
			const size_t k = floor_log2(hi - lo + 1);
			ans = std::min(ans, std::min(p[table[k][lo]], p[table[k][hi + 1 - (1ULL << k)]]));
		}
		return ans;
	}

	size_t space() const
	{
		size_t total = sizeof(*this);

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(uint32_t);

		total += prefixMin.capacity() * sizeof(uint32_t);
		total += suffixMin.capacity() * sizeof(uint32_t);

		return total;
	}
};

template <size_t BlockSize>
struct Cartesian
{
	static std::string name() { return "FiCart" + std::to_string(BlockSize); }

	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;

	std::vector<std::vector<uint32_t>> table;

	std::vector<uint32_t> blockShape;
	std::vector<uint8_t> lookup;

	static constexpr size_t QueriesPerBlock = BlockSize * (BlockSize + 1) / 2;

	static size_t inBlockIndex(size_t a, size_t b)
	{
		return a * BlockSize - (a * (a - 1)) / 2 + (b - a);
	}

	static Cartesian build(const std::vector<uint64_t> &data)
	{
		Cartesian rmq;
		rmq.data = &data;

		size_t n = data.size();
		size_t amountOfBlocks = (n + BlockSize - 1) / BlockSize;

		size_t numberOfLevels = 0;
		for (size_t x = amountOfBlocks; x > 0; x >>= 1)
			++numberOfLevels;

		rmq.table.resize(numberOfLevels);
		rmq.table[0].resize(amountOfBlocks);
		rmq.blockShape.resize(amountOfBlocks);

		std::unordered_map<uint64_t, uint32_t> rowOfCode;

		uint64_t stack[BlockSize];

		// level 0 + Cartesian tree shapes
		for (size_t block = 0; block < amountOfBlocks; ++block)
		{
			size_t start = block * BlockSize;
			size_t end = std::min(start + BlockSize, n);
			size_t len = end - start;

			uint64_t code = 0;
			size_t top = 0;

			for (size_t i = 0; i < BlockSize; ++i)
			{
				uint64_t v = (start + i < end) ? data[start + i] : UINT64_MAX;

				while (top > 0 && stack[top - 1] > v)
				{
					--top;
					code <<= 1;
				}

				stack[top++] = v;
				code = (code << 1) | 1;
			}

			code <<= top;

			auto it = rowOfCode.find(code);
			uint32_t row;

			if (it != rowOfCode.end())
			{
				row = it->second;
			}
			else
			{
				row = static_cast<uint32_t>(rmq.lookup.size() / QueriesPerBlock);
				rowOfCode.emplace(code, row);
				rmq.lookup.resize(rmq.lookup.size() + QueriesPerBlock);

				for (size_t a = 0; a < BlockSize; ++a)
				{
					size_t argmin = a;
					uint64_t best = (start + a < end) ? data[start + a] : UINT64_MAX;

					for (size_t b = a; b < BlockSize; ++b)
					{
						uint64_t v = (start + b < end) ? data[start + b] : UINT64_MAX;

						if (v < best)
						{
							best = v;
							argmin = b;
						}

						rmq.lookup[row * QueriesPerBlock + inBlockIndex(a, b)] =
							static_cast<uint8_t>(argmin);
					}
				}
			}

			rmq.blockShape[block] = row;
			rmq.table[0][block] =
				start + rmq.lookup[row * QueriesPerBlock + inBlockIndex(0, len - 1)];
		}

		// sparse table
		for (size_t level = 1; level < numberOfLevels; ++level)
		{
			size_t len = 1ULL << level;
			size_t half = len >> 1;
			size_t L = amountOfBlocks + 1 - len;

			rmq.table[level].resize(L);

			for (size_t j = 0; j < L; ++j)
			{
				size_t left = rmq.table[level - 1][j];
				size_t right = rmq.table[level - 1][j + half];

				if (data[left] <= data[right])
					rmq.table[level][j] = left;
				else
					rmq.table[level][j] = right;
			}
		}
		rmq.lookup.shrink_to_fit();
		return rmq;
	}

	uint64_t query(size_t l, size_t r) const
	{
		const uint64_t *p = data->data();
		const size_t bl = l / BlockSize, br = r / BlockSize;
		const size_t ls = bl * BlockSize;

		if (bl == br)
			return p[ls + lookup[blockShape[bl] * QueriesPerBlock + inBlockIndex(l - ls, r - ls)]];

		const size_t rs = br * BlockSize;

		uint64_t ans = std::min(
			p[ls + lookup[blockShape[bl] * QueriesPerBlock + inBlockIndex(l - ls, BlockSize - 1)]],
			p[rs + lookup[blockShape[br] * QueriesPerBlock + inBlockIndex(0, r - rs)]]);

		const size_t lo = bl + 1, hi = br - 1;
		if (lo <= hi)
		{
			const size_t k = floor_log2(hi - lo + 1);
			ans = std::min(ans, std::min(p[table[k][lo]], p[table[k][hi + 1 - (1ULL << k)]]));
		}

		return ans;
	}

	size_t space() const
	{
		size_t total = sizeof(*this);

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(uint32_t);

		total += blockShape.capacity() * sizeof(uint32_t);
		total += lookup.capacity() * sizeof(uint8_t);

		return total;
	}
};

template <size_t CCOMP>
struct AdaptiveCartesian
{
	size_t blockSize;
	size_t queriesPerBlock;

	static std::string name()
	{
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2)
			<< (CCOMP / 100.0);
		return "AdCart" + oss.str();
	}

	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;

	std::vector<std::vector<uint32_t>> table;

	std::vector<uint32_t> blockShape;
	std::vector<uint8_t> lookup;

	size_t inBlockIndex(size_t a, size_t b) const
	{
		return a * blockSize - (a * (a - 1)) / 2 + (b - a);
	}

	static AdaptiveCartesian build(const std::vector<uint64_t> &data)
	{
		AdaptiveCartesian rmq;

		constexpr double C = CCOMP / 100.0;

		rmq.blockSize = std::min<size_t>(
			32,
			std::max<size_t>(1, static_cast<size_t>(C * std::log2(data.size()))));
		rmq.queriesPerBlock = rmq.blockSize * (rmq.blockSize + 1) / 2;
		rmq.data = &data;

		size_t n = data.size();
		size_t amountOfBlocks = (n + rmq.blockSize - 1) / rmq.blockSize;

		size_t numberOfLevels = 0;
		for (size_t x = amountOfBlocks; x > 0; x >>= 1)
			++numberOfLevels;

		rmq.table.resize(numberOfLevels);
		rmq.table[0].resize(amountOfBlocks);
		rmq.blockShape.resize(amountOfBlocks);

		std::unordered_map<uint64_t, uint32_t> rowOfCode;

		std::vector<uint64_t> stack(rmq.blockSize);

		// level 0 + Cartesian tree shapes
		for (size_t block = 0; block < amountOfBlocks; ++block)
		{
			size_t start = block * rmq.blockSize;
			size_t end = std::min(start + rmq.blockSize, n);
			size_t len = end - start;

			uint64_t code = 0;
			size_t top = 0;

			for (size_t i = 0; i < rmq.blockSize; ++i)
			{
				uint64_t v = (start + i < end) ? data[start + i] : UINT64_MAX;

				while (top > 0 && stack[top - 1] > v)
				{
					--top;
					code <<= 1;
				}

				stack[top++] = v;
				code = (code << 1) | 1;
			}

			code <<= top;

			auto it = rowOfCode.find(code);
			uint32_t row;

			if (it != rowOfCode.end())
			{
				row = it->second;
			}
			else
			{
				row = static_cast<uint32_t>(rmq.lookup.size() / rmq.queriesPerBlock);
				rowOfCode.emplace(code, row);
				rmq.lookup.resize(rmq.lookup.size() + rmq.queriesPerBlock);

				for (size_t a = 0; a < rmq.blockSize; ++a)
				{
					size_t argmin = a;
					uint64_t best = (start + a < end) ? data[start + a] : UINT64_MAX;

					for (size_t b = a; b < rmq.blockSize; ++b)
					{
						uint64_t v = (start + b < end) ? data[start + b] : UINT64_MAX;

						if (v < best)
						{
							best = v;
							argmin = b;
						}

						rmq.lookup[row * rmq.queriesPerBlock + rmq.inBlockIndex(a, b)] =
							static_cast<uint8_t>(argmin);
					}
				}
			}

			rmq.blockShape[block] = row;
			rmq.table[0][block] = start + rmq.lookup[row * rmq.queriesPerBlock + rmq.inBlockIndex(0, len - 1)];
		}

		// sparse table
		for (size_t level = 1; level < numberOfLevels; ++level)
		{
			size_t len = 1ULL << level;
			size_t half = len >> 1;
			size_t L = amountOfBlocks + 1 - len;

			rmq.table[level].resize(L);

			for (size_t j = 0; j < L; ++j)
			{
				size_t left = rmq.table[level - 1][j];
				size_t right = rmq.table[level - 1][j + half];

				if (data[left] <= data[right])
					rmq.table[level][j] = left;
				else
					rmq.table[level][j] = right;
			}
		}
		rmq.lookup.shrink_to_fit();
		return rmq;
	}

	uint64_t query(size_t l, size_t r) const
	{
		const uint64_t *p = data->data();
		const size_t bl = l / blockSize, br = r / blockSize;
		const size_t ls = bl * blockSize;

		if (bl == br)
			return p[ls + lookup[blockShape[bl] * queriesPerBlock + inBlockIndex(l - ls, r - ls)]];

		const size_t rs = br * blockSize;

		uint64_t ans = std::min(
			p[ls + lookup[blockShape[bl] * queriesPerBlock + inBlockIndex(l - ls, blockSize - 1)]],
			p[rs + lookup[blockShape[br] * queriesPerBlock + inBlockIndex(0, r - rs)]]);

		const size_t lo = bl + 1, hi = br - 1;
		if (lo <= hi)
		{
			const size_t k = floor_log2(hi - lo + 1);
			ans = std::min(ans, std::min(p[table[k][lo]], p[table[k][hi + 1 - (1ULL << k)]]));
		}
		return ans;
	}

	size_t space() const
	{
		size_t total = sizeof(*this);

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(uint32_t);

		total += blockShape.capacity() * sizeof(uint32_t);
		total += lookup.capacity() * sizeof(uint8_t);

		return total;
	}
};

struct Input
{
	std::vector<uint64_t> data;
	std::vector<std::pair<size_t, size_t>> queries;
};

// Read the given input file.
Input read_input(const std::filesystem::path &file)
{
	std::ifstream f(file);
	size_t n, q;
	f >> n >> q;
	Input input;
	input.data.resize(n);
	for (auto &v : input.data)
		f >> v;
	input.queries.resize(q);
	for (auto &[l, r] : input.queries)
		f >> l >> r;
	return input;
}

// Bench the given RMQ implementation on the given input, and print results in CSV format.
template <typename RMQ>
void bench(const Input &input)
{
	std::cerr << std::setw(10) << input.data.size() << "\t" << std::setw(20) << RMQ::name() << "\t";

	size_t max_n = RMQ::max_n();

	if (input.data.size() > max_n)
	{
		std::cerr << "skipped\n";
		return;
	}

	auto rmq = RMQ::build(input.data);
	std::cerr << std::setw(10) << rmq.space() << "\t";

	auto start = std::chrono::high_resolution_clock::now();
	uint64_t sum = 0;
	for (auto &[l, r] : input.queries)
		sum += rmq.query(l, r);
	auto end = std::chrono::high_resolution_clock::now();

	double elapsed =
		static_cast<double>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) /
		static_cast<double>(input.queries.size());

	std::cout << input.data.size() << "," << input.queries.size() << "," << RMQ::name() << ","
			  << rmq.space() << "," << sum << "," << elapsed << "\n";
	std::cerr << std::setw(3) << (sum % 1000) << "\t" << std::fixed << std::setprecision(2)
			  << elapsed << "ns/q\n";
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		std::cerr << "Usage: rmq-cpp <input_dir>\n";
		return 1;
	}

	std::cout << "n,q,name,space,sum,time\n";

	std::filesystem::path file_or_dir(argv[1]);
	std::cerr << "Reading input from " << file_or_dir << " ..\n";

	std::vector<Input> inputs;
	if (std::filesystem::is_regular_file(file_or_dir))
	{
		inputs.push_back(read_input(file_or_dir));
	}
	else
	{
		for (auto &entry : std::filesystem::directory_iterator(file_or_dir))
		{
			if (entry.path().extension() == ".in")
				inputs.push_back(read_input(entry.path()));
		}
		std::sort(inputs.begin(), inputs.end(),
				  [](const Input &a, const Input &b)
				  { return a.data.size() < b.data.size(); });
	}

	for (const auto &input : inputs)
	{
		bench<OnTheFlyNaive>(input);
		bench<PrecomputedNaive>(input);

		bench<SparseTable>(input);
		bench<SegmentTree>(input);

		bench<Blocks<64>>(input);

		bench<SqrtAdaptiveBlocksPrecompute<250>>(input);
		bench<AdaptiveBlocksPrecompute<20000>>(input);

		bench<Cartesian<8>>(input);
		bench<AdaptiveCartesian<50>>(input);

		/* comparison for caretsian fixed size 8 and adaptive constant 0.5 wins
		bench<Cartesian<2>>(input);
		bench<Cartesian<4>>(input);
		bench<Cartesian<8>>(input);
		bench<Cartesian<16>>(input);
		bench<Cartesian<32>>(input);

		bench<AdaptiveCartesian<25>>(input);
		bench<AdaptiveCartesian<50>>(input);
		bench<AdaptiveCartesian<100>>(input);
		bench<AdaptiveCartesian<150>>(input);
		bench<AdaptiveCartesian<200>>(input);
		*/

		/* comparrison for the final decision for precompute blocks comparing fixed size - C*log(n) * C*√n
		2.5*√n wins fixed size is unreliable for adaptive 200 is good
		bench<SqrtAdaptiveBlocksPrecompute<200>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<250>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<300>>(input);

		bench<BlocksPrecompute<2048 * 2 * 2>>(input);
		bench<BlocksPrecompute<2048 * 2 * 2 * 2>>(input);
		bench<BlocksPrecompute<2048 * 2 * 2 * 2 * 2>>(input);

		bench<AdaptiveBlocksPrecompute<15000>>(input);
		bench<AdaptiveBlocksPrecompute<17500>>(input);
		bench<AdaptiveBlocksPrecompute<20000>>(input);
		*/

		/* data that was used to asses C*log(n) size for precompute blocks
		bench<AdaptiveBlocksPrecompute<100>>(input);
		bench<AdaptiveBlocksPrecompute<200>>(input);
		bench<AdaptiveBlocksPrecompute<300>>(input);
		bench<AdaptiveBlocksPrecompute<400>>(input);
		bench<AdaptiveBlocksPrecompute<500>>(input);
		bench<AdaptiveBlocksPrecompute<600>>(input);
		bench<AdaptiveBlocksPrecompute<700>>(input);
		bench<AdaptiveBlocksPrecompute<800>>(input);
		bench<AdaptiveBlocksPrecompute<900>>(input);
		bench<AdaptiveBlocksPrecompute<1000>>(input);
		bench<AdaptiveBlocksPrecompute<1500>>(input);
		bench<AdaptiveBlocksPrecompute<2000>>(input);
		bench<AdaptiveBlocksPrecompute<3000>>(input);
		bench<AdaptiveBlocksPrecompute<4000>>(input);
		bench<AdaptiveBlocksPrecompute<5000>>(input);
		bench<AdaptiveBlocksPrecompute<7500>>(input);
		bench<AdaptiveBlocksPrecompute<10000>>(input);
		bench<AdaptiveBlocksPrecompute<12500>>(input);
		bench<AdaptiveBlocksPrecompute<15000>>(input);
		bench<AdaptiveBlocksPrecompute<17500>>(input);
		bench<AdaptiveBlocksPrecompute<20000>>(input);
		*/

		/* data that was used to asses fixed size for precompute blocks
		bench<BlocksPrecompute<2>>(input);
		bench<BlocksPrecompute<4>>(input);
		bench<BlocksPrecompute<8>>(input);
		bench<BlocksPrecompute<16>>(input);
		bench<BlocksPrecompute<32>>(input);
		bench<BlocksPrecompute<64>>(input);
		bench<BlocksPrecompute<128>>(input);
		bench<BlocksPrecompute<256>>(input);
		bench<BlocksPrecompute<512>>(input);
		bench<BlocksPrecompute<1024>>(input);
		bench<BlocksPrecompute<2048>>(input);
		bench<BlocksPrecompute<2048 * 2>>(input);
		bench<BlocksPrecompute<2048 * 2 * 2>>(input);
		bench<BlocksPrecompute<2048 * 2 * 2 * 2>>(input);
		bench<BlocksPrecompute<2048 * 2 * 2 * 2 * 2>>(input);
		bench<BlocksPrecompute<2048 * 2 * 2 * 2 * 2 * 2>>(input);
		*/

		/* 2.5 is the best for SqrtAdaptiveBlocksPrecompute
		bench<SqrtAdaptiveBlocksPrecompute<25>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<50>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<75>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<100>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<150>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<200>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<250>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<300>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<350>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<400>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<450>>(input);
		bench<SqrtAdaptiveBlocksPrecompute<500>>(input);
		*/

		/*
		these for testing out the block size it turns out fixed64 is good
		bench<Blocks<16>>(input);
		bench<Blocks<32>>(input);
		bench<Blocks<64>>(input);
		bench<Blocks<128>>(input);


		bench<AdaptiveBlocks<100>>(input);
		bench<AdaptiveBlocks<200>>(input);
		bench<AdaptiveBlocks<225>>(input);
		bench<AdaptiveBlocks<250>>(input);
		bench<AdaptiveBlocks<275>>(input);
		bench<AdaptiveBlocks<300>>(input);
		bench<AdaptiveBlocks<400>>(input);
		bench<AdaptiveBlocks<500>>(input);
		*/
	}

	return 0;
}
