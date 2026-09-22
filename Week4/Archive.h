#pragma once

#include <filesystem>
#include <fstream>

class FArchive
{
public:
	virtual ~FArchive() = default;

	virtual bool IsLoading() const = 0;
	virtual bool IsSaving() const = 0;

	// void* : pointer to memory address, don't care what data type lives there
	// just write whatever is in that address, with its length.
	virtual void Serialize(void* V, uint32 Length) = 0;

	template<typename T>
	FArchive& operator<<(T& Value)
	{
		// standard layout : memory arrangement follows strict, predictable rules guaranteed by C++ standard
		static_assert(std::is_standard_layout<T>::value, "Must be a Standard Layout Type!");
		Serialize(&Value, sizeof(T));
		return *this;
	}
};

class FArchiveFileWriter : public FArchive
{
public:
	FArchiveFileWriter(const std::filesystem::path& FilePath)
	{
		// for writing, binary format, truncate (덮어쓰기)
		Stream.open(FilePath, std::ios::out | std::ios::binary | std::ios::trunc);
	}

	bool IsLoading() const { return false; }
	bool IsSaving() const { return true; }
	bool IsValid() const { return Stream.is_open(); }

	void Serialize(void* V, uint32 Length) override
	{
		if (IsValid())
		{
			// can't write void*, so cast it as char* (raw memory reinterpretation)
			Stream.write(reinterpret_cast<const char*>(V), Length);
		}
	}

private:
	std::ofstream Stream;
};

class FArchiveFileReader : public FArchive
{
public:
	FArchiveFileReader(const std::filesystem::path& FilePath)
	{
		Stream.open(FilePath, std::ios::in | std::ios::binary);
	}

	bool IsLoading() const { return true; }
	bool IsSaving() const { return false; }
	bool IsValid() const { return Stream.is_open(); }

	void Serialize(void* V, uint32 Length) override
	{
		if (IsValid())
		{
			Stream.read(reinterpret_cast<char*>(V), Length);
		}
	}

private:
	std::ifstream Stream;
};