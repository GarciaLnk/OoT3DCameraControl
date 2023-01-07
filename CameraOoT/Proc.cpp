#include "Proc.h"
#include <memory>

DWORD GetProcId(const wchar_t* proc_name)
{
	DWORD proc_id = 0;
	const HANDLE h_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (h_snap == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	PROCESSENTRY32 proc_entry;
	proc_entry.dwSize = sizeof(proc_entry);

	if (Process32First(h_snap, &proc_entry))
	{
		do
		{
			if (!_wcsicmp(proc_entry.szExeFile, proc_name))
			{
				proc_id = proc_entry.th32ProcessID;
				break;
			}
		} while (Process32Next(h_snap, &proc_entry));
	}

	CloseHandle(h_snap);
	return proc_id;
}

struct PartData
{
	int32_t mask = 0;
	__m128i needle{};

	PartData()
	{
		memset(&needle, 0, sizeof(needle));
	}
};

// Credits to @DarthTon for this function (source: https://github.com/learn-more/findpattern-bench/blob/master/patterns/DarthTon.h)
const uint8_t* Search(const uint8_t* data, const uint32_t size, const uint8_t* pattern, const char* mask)
{
	const uint8_t* result = nullptr;
	auto len = strlen(mask);
	const auto first = strchr(mask, '?');
	const size_t len2 = (first != nullptr) ? (first - mask) : len;
	const auto firstlen = min(len2, 16);
	const intptr_t num_parts = (len < 16 || len % 16) ? (len / 16 + 1) : (len / 16);
	std::array<PartData, 4> parts;

	for (intptr_t i = 0; i < num_parts; ++i, len -= 16)
	{
		for (size_t j = 0; j < min(len, 16) - 1; ++j)
			if (mask[16 * i + j] == 'x')
				_bittestandset(reinterpret_cast<LONG*>(&parts[i].mask), j);

		parts[i].needle = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pattern + i * 16));
	}

	bool abort = false;

#pragma omp parallel for
	for (intptr_t i = 0; i < static_cast<intptr_t>(size) / 32 - 1; ++i)
	{
#pragma omp flush(abort)
		if (!abort)
		{
			if (const auto block = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data) + i); _mm256_testz_si256(block, block))
				continue;

			auto offset = _mm_cmpestri(parts[0].needle, firstlen, _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i * 32)), 16,
				_SIDD_CMP_EQUAL_ORDERED);
			if (offset == 16)
			{
				offset += _mm_cmpestri(parts[0].needle, firstlen, _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i * 32 + 16)),
					16, _SIDD_CMP_EQUAL_ORDERED);
				if (offset == 32)
					continue;
			}

			for (intptr_t j = 0; j < num_parts; ++j)
			{
				const auto hay = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + (2 * i + j) * 16 + offset));
				if (const auto bitmask = _mm_movemask_epi8(_mm_cmpeq_epi8(hay, parts[j].needle)); (bitmask & parts[j].mask) != parts[j].mask)
					goto next;
			}

			result = data + 32 * i + offset;
			abort = true;
#pragma omp flush(abort)
		}

	next:;
	}

	return result;
}

uintptr_t SearchInProcessMemory(const HANDLE h_process, const uint8_t* pattern, const char* mask)
{
	constexpr int buffer_size = 1024 * 1024;
	const auto buffer = std::make_unique<uint8_t[]>(buffer_size);
	uintptr_t result = 0x0;
	uint8_t* read_addr = nullptr;

	MEMORY_BASIC_INFORMATION mem_info;
	while (VirtualQueryEx(h_process, reinterpret_cast<void*>(read_addr), &mem_info, sizeof(mem_info)))
	{
		if (mem_info.Protect == PAGE_NOACCESS)
		{
			read_addr += mem_info.RegionSize;
			continue;
		}
		break;
	}

	if (read_addr == nullptr)
		return 0x0;

	while (result == 0)
	{
		SIZE_T bytes_read = 0;
		if (const BOOL success = ReadProcessMemory(h_process, read_addr, buffer.get(), buffer_size, &bytes_read); !success || bytes_read == 0)
		{
			read_addr += buffer_size;
			continue;
		}

		if (const void* ptr = Search(buffer.get(), bytes_read, pattern, mask); ptr != nullptr)
		{
			result = reinterpret_cast<uintptr_t>(read_addr) + (reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(buffer.get()));
			break;
		}

		read_addr += buffer_size;
	}

	return result;
}
