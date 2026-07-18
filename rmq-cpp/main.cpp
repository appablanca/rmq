#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <algorithm>
#include <stdexcept>
#include <limits>

// RMQ interface (duck-typed via templates):
//
//   static std::string name();
//   static size_t max_n();               // optional, defaults to SIZE_MAX
//   static RMQ build(const std::vector<uint64_t>& data);
//   size_t space() const;
//   uint64_t query(size_t l, size_t r) const;

// Trivial implementation that computes each query on the fly.
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
		const size_t k = static_cast<size_t>(std::log2(n));

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
		size_t k = static_cast<size_t>(std::log2(len));

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
		const size_t k = static_cast<uint64_t>(std::log2(n));

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
	static std::string name()
	{
		return "Block" + std::to_string(BlockSize);
	}

	static size_t max_n()
	{
		return SIZE_MAX;
	}

	const std::vector<uint64_t> *data = nullptr;

	std::vector<std::vector<size_t>> table;
	static Blocks build(const std::vector<uint64_t> &data)
	{
		const size_t n = data.size();

		Blocks rmq;
		rmq.data = &data;

		if (n == 0)
			return rmq;

		const size_t numberOfBlocks =
			(n + BlockSize - 1) / BlockSize;

		size_t numberOfLevels = 0;

		for (size_t x = numberOfBlocks; x > 0; x >>= 1)
			++numberOfLevels;

		rmq.table.resize(numberOfLevels);

		rmq.table[0].resize(numberOfBlocks);

		for (size_t block = 0; block < numberOfBlocks; ++block)
		{
			const size_t begin = block * BlockSize;
			const size_t end = std::min(begin + BlockSize, n);

			size_t minimumIndex = begin;

			for (size_t i = begin + 1; i < end; ++i)
			{
				if (data[i] < data[minimumIndex])
					minimumIndex = i;
			}

			rmq.table[0][block] = minimumIndex;
		}

		for (size_t level = 1; level < numberOfLevels; ++level)
		{
			const size_t blockSpan = size_t{1} << level;
			const size_t halfSpan = blockSpan >> 1;

			const size_t currentLevelSize =
				numberOfBlocks + 1 - blockSpan;

			rmq.table[level].resize(currentLevelSize);

			for (size_t j = 0; j < currentLevelSize; ++j)
			{
				const size_t leftIndex =
					rmq.table[level - 1][j];

				const size_t rightIndex =
					rmq.table[level - 1][j + halfSpan];

				rmq.table[level][j] =
					data[leftIndex] <= data[rightIndex]
						? leftIndex
						: rightIndex;
			}
		}

		return rmq;
	}

	static size_t floor_log2(size_t x)

	{

		size_t result = 0;

		while (x > 1)

		{

			x >>= 1;

			++result;
		}

		return result;
	}
	size_t query_index(size_t l, size_t r) const
	{
		if (data == nullptr || l > r || r >= data->size())
			throw std::out_of_range("Invalid RMQ range");

		const size_t leftBlock = l / BlockSize;
		const size_t rightBlock = r / BlockSize;

		size_t minimumIndex = l;

		auto consider = [&](size_t index)
		{
			if ((*data)[index] < (*data)[minimumIndex] ||
				((*data)[index] == (*data)[minimumIndex] &&
				 index < minimumIndex))
			{
				minimumIndex = index;
			}
		};

		// Sorgu tamamen tek blok içerisinde.
		if (leftBlock == rightBlock)
		{
			for (size_t i = l + 1; i <= r; ++i)
				consider(i);

			return minimumIndex;
		}

		// Sol kısmi blok.
		const size_t leftEnd =
			std::min((leftBlock + 1) * BlockSize, data->size());

		for (size_t i = l; i < leftEnd; ++i)
			consider(i);

		// Sağ kısmi blok.
		const size_t rightBegin = rightBlock * BlockSize;

		for (size_t i = rightBegin; i <= r; ++i)
			consider(i);

		// Aradaki tamamen kapsanan bloklar.
		const size_t firstFullBlock = leftBlock + 1;
		const size_t lastFullBlock = rightBlock - 1;

		if (firstFullBlock <= lastFullBlock)
		{
			const size_t numberOfFullBlocks =
				lastFullBlock - firstFullBlock + 1;

			const size_t level =
				floor_log2(numberOfFullBlocks);

			const size_t span =
				size_t{1} << level;

			const size_t leftIndex =
				table[level][firstFullBlock];

			const size_t rightIndex =
				table[level][lastFullBlock + 1 - span];

			consider(leftIndex);
			consider(rightIndex);
		}

		return minimumIndex;
	}
	uint64_t query(size_t l, size_t r) const

	{

		return (*data)[query_index(l, r)];
	}

	size_t space() const

	{

		size_t bytes = sizeof(*this);

		// Dynamic storage for the inner vector objects.

		bytes +=

			table.capacity() *

			sizeof(std::vector<size_t>);

		for (const auto &row : table)

		{

			bytes +=

				row.capacity() * sizeof(size_t);
		}

		return bytes;
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
		bench<OnTheFlyNaive>(input);
		bench<PrecomputedNaive>(input);
		bench<SparseTable>(input);
		bench<SegmentTree>(input);

		bench<Blocks<2>>(input);
		bench<Blocks<4>>(input);
		bench<Blocks<8>>(input);
		bench<Blocks<16>>(input);
		bench<Blocks<32>>(input);
	}

	return 0;
}
