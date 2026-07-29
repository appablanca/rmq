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
inline size_t floor_log2(size_t x)
{
	return 8 * sizeof(size_t) - 1 - __builtin_clzll(x);
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
	static std::string name() { return "OnTheFlyNaive"; }
	// NOTE: Improved implementations should simply return size_t::MAX.
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
	static std::string name() { return "PrecomputedNaive"; }

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
	static std::string name() { return "SparseTable"; }
	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;
	std::vector<std::vector<size_t>> table;

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
			bytes += row.size() * sizeof(size_t);
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
	static std::string name() { return "SegmentTree"; }

	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;
	std::vector<std::vector<size_t>> tree;

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

		size_t bytes = 0;

		for (const auto &row : tree)
		{
			bytes += row.size() * sizeof(size_t);
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
	std::vector<std::vector<size_t>> table;

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
		uint64_t leftMin = UINT64_MAX;
		uint64_t middleMin = UINT64_MAX;
		uint64_t rightMin = UINT64_MAX;

		// if same block
		if (l / BlockSize == r / BlockSize)
		{
			uint64_t ans = UINT64_MAX;
			for (size_t i = l; i <= r; ++i)
				ans = std::min(ans, (*data)[i]);
			return ans;
		}
		// left query
		size_t leftEnd = std::min(((l / BlockSize) + 1) * BlockSize - 1, r);
		for (size_t i = l; i <= leftEnd; ++i)
			leftMin = std::min(leftMin, (*data)[i]);
		// middle query
		size_t lp = (l + BlockSize - 1) / BlockSize;

		size_t rp = r / BlockSize;
		if (lp < rp)
		{
			--rp;
			size_t len = rp - lp + 1;
			size_t k = floor_log2(len);
			size_t leftIdx = table[k][lp];
			size_t rightIdx = table[k][rp - (1ULL << k) + 1];
			middleMin = std::min((*data)[leftIdx], (*data)[rightIdx]);
		}

		// right query
		size_t lastBlock = r / BlockSize;

		size_t rightStart = lastBlock * BlockSize;
		for (size_t i = rightStart; i <= r; ++i)
			rightMin = std::min(rightMin, (*data)[i]);

		return std::min({leftMin, middleMin, rightMin});
	}

	size_t space() const
	{
		size_t total = 0;

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(size_t);

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
	std::vector<std::vector<size_t>> table;

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
		uint64_t leftMin = UINT64_MAX;
		uint64_t middleMin = UINT64_MAX;
		uint64_t rightMin = UINT64_MAX;

		// same block
		if (l / blockSize == r / blockSize)
		{
			uint64_t ans = UINT64_MAX;
			for (size_t i = l; i <= r; ++i)
				ans = std::min(ans, (*data)[i]);
			return ans;
		}

		// left partial block
		size_t leftEnd = std::min(((l / blockSize) + 1) * blockSize - 1, r);

		for (size_t i = l; i <= leftEnd; ++i)
			leftMin = std::min(leftMin, (*data)[i]);

		// middle full blocks
		size_t lp = (l + blockSize - 1) / blockSize;
		size_t rp = r / blockSize;

		if (lp < rp)
		{
			--rp;

			size_t len = rp - lp + 1;
			size_t k = floor_log2(len);

			size_t leftIdx = table[k][lp];
			size_t rightIdx = table[k][rp - (1ULL << k) + 1];

			middleMin = std::min((*data)[leftIdx], (*data)[rightIdx]);
		}

		// right partial block
		size_t lastBlock = r / blockSize;
		size_t rightStart = lastBlock * blockSize;

		for (size_t i = rightStart; i <= r; ++i)
			rightMin = std::min(rightMin, (*data)[i]);

		return std::min({leftMin, middleMin, rightMin});
	}

	size_t space() const
	{
		size_t total = sizeof(*this);

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(size_t);

		return total;
	}
};

template <size_t BlockSize>
struct BlocksPrecompute
{
	static std::string name() { return "FiBlPre" + std::to_string(BlockSize); }

	static size_t max_n() { return SIZE_MAX; }

	const std::vector<uint64_t> *data = nullptr;

	std::vector<std::vector<size_t>> table;

	std::vector<size_t> prefixMin;
	std::vector<size_t> suffixMin;

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

			// block minimum
			size_t blockMin = start;
			for (size_t i = start + 1; i < end; ++i)
			{
				if (data[i] < data[blockMin])
					blockMin = i;
			}
			rmq.table[0][block] = blockMin;

			// prefix minima
			rmq.prefixMin[start] = start;
			for (size_t i = start + 1; i < end; ++i)
			{
				size_t prev = rmq.prefixMin[i - 1];
				if (data[i] < data[prev])
					rmq.prefixMin[i] = i;
				else
					rmq.prefixMin[i] = prev;
			}

			// suffix minima
			rmq.suffixMin[end - 1] = end - 1;
			for (size_t i = end - 1; i > start; --i)
			{
				size_t next = rmq.suffixMin[i];
				if (data[i - 1] <= data[next])
					rmq.suffixMin[i - 1] = i - 1;
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
		// same block
		if (l / BlockSize == r / BlockSize)
		{
			uint64_t ans = UINT64_MAX;
			for (size_t i = l; i <= r; ++i)
				ans = std::min(ans, (*data)[i]);
			return ans;
		}

		uint64_t leftMin = (*data)[suffixMin[l]];
		uint64_t rightMin = (*data)[prefixMin[r]];
		uint64_t middleMin = UINT64_MAX;

		size_t lp = (l + BlockSize - 1) / BlockSize;
		size_t rp = r / BlockSize;

		if (lp < rp)
		{
			--rp;

			size_t len = rp - lp + 1;
			size_t k = floor_log2(len);

			size_t leftIdx = table[k][lp];
			size_t rightIdx = table[k][rp - (1ULL << k) + 1];

			middleMin = std::min((*data)[leftIdx], (*data)[rightIdx]);
		}

		return std::min({leftMin, middleMin, rightMin});
	}

	size_t space() const
	{
		size_t total = 0;

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(size_t);

		total += prefixMin.capacity() * sizeof(size_t);
		total += suffixMin.capacity() * sizeof(size_t);

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

	std::vector<std::vector<size_t>> table;

	std::vector<size_t> prefixMin;
	std::vector<size_t> suffixMin;

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

			// block minimum
			size_t blockMin = start;
			for (size_t i = start + 1; i < end; ++i)
			{
				if (data[i] < data[blockMin])
					blockMin = i;
			}
			rmq.table[0][block] = blockMin;

			// prefix minima
			rmq.prefixMin[start] = start;
			for (size_t i = start + 1; i < end; ++i)
			{
				size_t prev = rmq.prefixMin[i - 1];
				if (data[i] < data[prev])
					rmq.prefixMin[i] = i;
				else
					rmq.prefixMin[i] = prev;
			}

			// suffix minima
			rmq.suffixMin[end - 1] = end - 1;
			for (size_t i = end - 1; i > start; --i)
			{
				size_t next = rmq.suffixMin[i];
				if (data[i - 1] <= data[next])
					rmq.suffixMin[i - 1] = i - 1;
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
		// same block
		if (l / blockSize == r / blockSize)
		{
			uint64_t ans = UINT64_MAX;
			for (size_t i = l; i <= r; ++i)
				ans = std::min(ans, (*data)[i]);
			return ans;
		}

		uint64_t leftMin = (*data)[suffixMin[l]];
		uint64_t rightMin = (*data)[prefixMin[r]];
		uint64_t middleMin = UINT64_MAX;

		size_t lp = (l + blockSize - 1) / blockSize;
		size_t rp = r / blockSize;

		if (lp < rp)
		{
			--rp;

			size_t len = rp - lp + 1;
			size_t k = floor_log2(len);

			size_t leftIdx = table[k][lp];
			size_t rightIdx = table[k][rp - (1ULL << k) + 1];

			middleMin = std::min((*data)[leftIdx], (*data)[rightIdx]);
		}

		return std::min({leftMin, middleMin, rightMin});
	}

	size_t space() const
	{
		size_t total = 0;

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(size_t);

		total += prefixMin.capacity() * sizeof(size_t);
		total += suffixMin.capacity() * sizeof(size_t);

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

	std::vector<std::vector<size_t>> table;

	std::vector<size_t> prefixMin;
	std::vector<size_t> suffixMin;

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

			// block minimum
			size_t blockMin = start;
			for (size_t i = start + 1; i < end; ++i)
			{
				if (data[i] < data[blockMin])
					blockMin = i;
			}
			rmq.table[0][block] = blockMin;

			// prefix minima
			rmq.prefixMin[start] = start;
			for (size_t i = start + 1; i < end; ++i)
			{
				size_t prev = rmq.prefixMin[i - 1];
				if (data[i] < data[prev])
					rmq.prefixMin[i] = i;
				else
					rmq.prefixMin[i] = prev;
			}

			// suffix minima
			rmq.suffixMin[end - 1] = end - 1;
			for (size_t i = end - 1; i > start; --i)
			{
				size_t next = rmq.suffixMin[i];
				if (data[i - 1] <= data[next])
					rmq.suffixMin[i - 1] = i - 1;
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
		// same block
		if (l / blockSize == r / blockSize)
		{
			uint64_t ans = UINT64_MAX;
			for (size_t i = l; i <= r; ++i)
				ans = std::min(ans, (*data)[i]);
			return ans;
		}

		uint64_t leftMin = (*data)[suffixMin[l]];
		uint64_t rightMin = (*data)[prefixMin[r]];
		uint64_t middleMin = UINT64_MAX;

		size_t lp = (l + blockSize - 1) / blockSize;
		size_t rp = r / blockSize;

		if (lp < rp)
		{
			--rp;

			size_t len = rp - lp + 1;
			size_t k = floor_log2(len);

			size_t leftIdx = table[k][lp];
			size_t rightIdx = table[k][rp - (1ULL << k) + 1];

			middleMin = std::min((*data)[leftIdx], (*data)[rightIdx]);
		}

		return std::min({leftMin, middleMin, rightMin});
	}

	size_t space() const
	{
		size_t total = 0;

		total += table.capacity() * sizeof(std::vector<size_t>);

		for (const auto &level : table)
			total += level.capacity() * sizeof(size_t);

		total += prefixMin.capacity() * sizeof(size_t);
		total += suffixMin.capacity() * sizeof(size_t);

		return total;
	}
};
// -------------------------------------------------------------
// TODO: Implement the RMQ interface for additional data structures.
// -------------------------------------------------------------

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
		/*


		bench<OnTheFlyNaive>(input);
		bench<PrecomputedNaive>(input);
		bench<SparseTable>(input);
		bench<SegmentTree>(input);
		bench<Blocks<64>>(input);
		if (input.data.size() >= 10000000)
		{
			bench<AdaptiveBlocks<225>>(input);
		}

		*/


		/*



		these for testing out the block size it turns out fixed64 is good for < 1000000
		anything over 1000000 2.25 works
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
